define arm6?

namespace arm6?
	define regs?
	namespace regs?
		repeat 16, i:0
			r#i? equ i
		end repeat
		sp? equ r13
		lr? equ r14
		pc? equ r15
	end namespace

	define sysregs?
	namespace sysregs?
		apsr?    equ 0
		iapsr?   equ 1
		eapsr?   equ 2
		xpsr?    equ 3
		ipsr?    equ 5
		epsr?    equ 6
		iepsr?   equ 7
		msp?     equ 8
		psp?     equ 9
		primask? equ 16
		control? equ 20
	end namespace

	element optype
	element offset:    optype + 0
	element immediate: optype + 1
	element register:  optype + 2

	iterate context, @src, @dst, @aux
		define context?
		calminstruction parse_operand#context? operand&
			match		[operand], operand
			jyes		parse_offset

			transform	operand, regs
			jno		parse_immediate
			compute		context.type, register
			compute		context.value, operand
			exit

		parse_immediate:
			compute		context.type, immediate
			compute		context.value, +operand
			exit

		parse_offset:
			compute		context.type, offset
			match		base=,offs, operand
			jyes		start
			match		base, operand
			compute		context.offtype, immediate
			compute		context.offvalue, 0
			jump		half

		start:	transform	offs, regs
			jno		immkind
			check		offs < 8
			jyes		cont
			arrange		error, =err 'offset must be low register'
			assemble	error
		cont:	compute		context.offtype, register
			compute		context.offvalue, offs
			jump		half
		immkind:
			compute		context.offtype, arm6.immediate
			compute		context.offvalue, +offs

		half:	transform	base, regs
			jyes		checkpc
			arrange		error, =err 'base register must be register'
			assemble	error
		checkpc:
			check		base <> arm6.regs.pc
			jyes		cont2
			arrange		error, =err 'pc not permitted for use as base register'
			assemble	error
		cont2:	compute		context.base, base
		end calminstruction
	end iterate
end namespace

; data processing

calminstruction adr? dst, label*
	local		address, error

	call		arm6.parse_operand@dst, dst
	check		arm6.@dst.value > 7
	jno		cont
	arrange		error, =err 'destination must be low register'
	assemble	error

cont:	compute		address, label - ($ + 4)
	check		address mod 4 = 0
	jyes		rangecheck
	arrange		error, =err 'label must be 4 aligned'
	assemble	error
rangecheck:
	check		address >= 0 & address <= 1020
	jyes		finish
	arrange		error, =err 'label is too far'
	assemble	error

finish:	local		op
	compute		address, address / 4
	compute		op, 0a000h or arm6.@dst.value shl 8 or address
	arrange		op, =dw op
	assemble	op
end calminstruction

iterate <name, regs, imm3, imm8>, \
adds, 1800h, 1c00h, 3000h,        \
subs, 1a00h, 1e00h, 3800h
	calminstruction name? dst, src*, aux:
		local		op, error

		call		arm6.parse_operand@dst, dst
		call		arm6.parse_operand@src, src

		check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
		jyes		cont
		arrange		error, =err 'destination must be low register'
		assemble	error

	cont:	check		arm6.@src.type relativeto arm6.immediate
		jyes		imm8_encoding
		check		arm6.@src.type relativeto arm6.register & arm6.@src.value < 8
		jyes		cont2
		arrange		error, =err 'source must be low register'
		assemble	error

	cont2:	match		, aux
		jyes		skip
		call		arm6.parse_operand@aux, aux
		check		arm6.@aux.type relativeto arm6.immediate
		jyes		imm3_encoding
		check		arm6.@aux.type relativeto arm6.register & arm6.@aux.value < 8
		jyes		reg_encoding
		arrange		error, =err 'third argument must be low register or immediate'
		assemble	error
	skip:	compute		arm6.@aux.value, arm6.@src.value
		compute		arm6.@src.value, arm6.@dst.value

	reg_encoding:
		compute		op, regs or arm6.@dst.value or arm6.@src.value shl 3 or \
				    arm6.@aux.value shl 6
		jump		finish

	imm3_encoding:
		check		arm6.@aux.value > 7
		jyes		imm3finish
		arrange		error, =err 'third argument must be 0 to 7'
		assemble	error
	imm3finish:
		compute		op, imm3 or arm6.@dst.value or arm6.@src.value shl 3 or \
				    arm6.@aux.value shl 6
		jump		finish

	imm8_encoding:
		check		arm6.@src.value < 256
		jyes		imm8finish
		arrange		error, =err 'immediate must be less than 256'
		assemble	error
	imm8finish:
		compute		op, imm8 or arm6.@dst.value shl 8 or arm6.@src.value

	finish:	arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

; add bigdst, bigsrc
; add SP, SP, imm7
; add dst, SP, imm8

calminstruction add? dst, src*, aux:
	local		op, error

	call		arm6.parse_operand@dst, dst
	call		arm6.parse_operand@src, src

	check		arm6.@dst.type relativeto arm6.register & \
			arm6.@src.type relativeto arm6.register
	jyes		nopc
	arrange		error, =err 'destination and source must be registers'
	assemble	error
nopc:	check		arm6.@dst.value <> arm6.regs.pc & arm6.@src.value <> arm6.regs.pc
	jyes		cont
	arrange		error, =err 'pc not permitted for use in this instruction'
	assemble	error

cont:	match		, aux
	jyes		noaux
	call		arm6.parse_operand@aux, aux
	check		arm6.@aux.type relativeto arm6.immediate
	jyes		cont2
	arrange		error, =err 'third argument must be immediate'
	assemble	error
cont2:	check		arm6.@aux.value mod 4 = 0
	jyes		auxcont
	arrange		error, =err 'immediate must be multiple of 4'
	assemble	error

auxcont:
	check		arm6.@src.value = arm6.regs.sp
	jyes		spcont
	arrange		error, =err 'source register must be sp'
	assemble	error
spcont: check		arm6.@dst.value = arm6.regs.sp
	jyes		spsp_encoding
	check		arm6.@dst.value < 8
	jyes		regsp_encoding
	arrange		error, =err 'destination must be low register'
	assemble	error

regsp_encoding:
	check		arm6.@aux.value < 1021
	jyes		regsp_finish
	arrange		error, =err 'immediate must be less than 1021'
	assemble	error

regsp_finish:
	compute		op, 0a800h or arm6.@dst.value shl 8 or arm6.@aux.value
	jump		finish

spsp_encoding:
	check		arm6.@dst.value < 509
	jyes		spsp_finish
	arrange		error, =err 'immediate must be less than 509'
	assemble	error

spsp_finish:
	compute		op, 0b000h or arm6.@dst.value
	jump		finish

noaux:	compute		op, 4400h or arm6.@src.value shl 3 or \
			    (arm6.@dst.value and 7h) or (arm6.@dst.value and 8h) shl 4

finish:	arrange		op, =dw op
	assemble	op
end calminstruction

calminstruction sub? dst, src*
	local		op, error
	
	call		arm6.parse_operand@dst, dst
	call		arm6.parse_operand@src, src

	check		arm6.@dst.type relativeto arm6.register
	jyes		cont
	arrange		error, =err 'destination must be a register'
	assemble	error
cont:	check		arm6.@src.type relativeto arm6.immediate
	jyes		cont2
	arrange		error, =err 'source must be immediate'
	assemble	error
cont2:	check		arm6.@src.value mod 4 = 0
	jyes		finish
	arrange		error, =err 'immediate must be multiple of 4, less than 509'
	assemble	error

finish:	compute		op, 0b080h or (arm6.@src.value / 4)
	arrange		op, =dw op
	assemble	op
end calminstruction

iterate <name, basecode>, \
ands,  4000h, 		  \
eors,  4040h, 		  \
adcs,  4140h, 		  \
sbcs,  4180h, 		  \
rors,  41c0h, 		  \
tst,   4200h, 		  \
rsbs,  4240h, 		  \
cmn,   42c0h, 		  \
orrs,  4300h, 		  \
muls,  4340h, 		  \
bics,  4380h, 		  \
mvns,  43c0h,		  \
sxtb,  0b240h,		  \
sxth,  0b200h,		  \
uxtb,  0b2c0h,		  \
uxth,  0b280h,		  \
rev,   0ba00h,		  \
rev16, 0ba40h,		  \
revsh, 0bac0h		  
	calminstruction name? dst, src*
		local		op, error

		call		arm6.parse_operand@dst, dst
		call		arm6.parse_operand@src, src

		check		arm6.@dst.type relativeto arm6.register & \
				arm6.@src.type relativeto arm6.register & \
				arm6.@dst.value < 8 & arm6.@src.value < 8
		jyes		finish
		arrange		error, =err 'destination and source must be low registers'
		assemble	error

	finish: compute 	op, basecode or \
				    arm6.@dst.value or arm6.@src.value shl 3
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

iterate <name, regbase, immbase>, \
lsls, 4080h, 0000h, 		  \
lsrs, 40c0h, 0800h, 		  \
asrs, 4100h, 1000h
	calminstruction name? dst, src*, imm:
		local		op, error

		call 		arm6.parse_operand@dst, dst
		call 		arm6.parse_operand@src, src

		check		arm6.@dst.type relativeto arm6.register & \ 
				arm6.@src.type relativeto arm6.register
		jyes		cont
		arrange		error, =err 'destination and source must be registers'
		assemble	error

	cont:	match		, imm
		jno		immediate_encoding

		check		arm6.@dst.value < 8 & arm6.@src.value < 8
		jyes		regfinish
		arrange		error, =err 'destination and source must be low registers'
		assemble	error
	regfinish:
		compute		op, regbase or arm6.@dst.value or arm6.@src.value shl 3
		arrange		op, =dw op
		assemble	op
		exit

	immediate_encoding:
		call		arm6.parse_operand@aux, imm
		check		arm6.@aux.type relativeto arm6.immediate
		jyes		immcheck
		arrange		error, =err 'shift expects third parameter as immediate'
		assemble	error
	immcheck:
		check		arm6.@aux.value < 32
		jyes		immfinish
		arrange		error, =err 'shift immediate must be below 32'
		assemble	error
	immfinish:
		compute		op, immbase or arm6.@dst.value or arm6.@src.value shl 3 \
				    or arm6.@aux.value shl 6
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

calminstruction cmp? a, b*
	local		op, error

	call		arm6.parse_operand@dst, a
	call		arm6.parse_operand@src, b

	check		arm6.@dst.type relativeto arm6.register
	jyes		cont
	arrange		error, =err 'destination must be register'
	assemble	error
cont:	check		arm6.@src.type relativeto arm6.register
	jno		isimm
	check		arm6.@src.value > 7 | arm6.@dst.value > 7
	jyes		bigreg_encoding
	jump		reg_encoding
isimm:	check		arm6.@src.type relativeto arm6.immediate
	jyes		imm_encoding
	arrange		error, =err 'source must be immediate or register'
	assemble	error

imm_encoding:
	check		arm6.@dst.value > 7
	jno		immcheck
	arrange		error, =err 'destination must be low register'
	assemble	error
immcheck:
	check		arm6.@src.value < 256
	jyes		immfinish
	arrange		error, =err 'immediate must be below 256'
	assemble	error
immfinish:
	compute		op, 2800h or arm6.@dst.value shl 8 or arm6.@src.value
	jump 		finish

bigreg_encoding:
	check		arm6.@dst.value <> arm6.regs.pc & arm6.@src.value <> arm6.regs.pc
	jyes		bigfinish
	arrange		error, =err 'pc not permitted for use in this instruction'
	assemble	error
bigfinish:
	compute		op, 4500h or arm6.@src.value shl 3 or \
			    (arm6.@dst.value and 7h) or (arm6.@dst.value and 8h) shl 4
	jump		finish

reg_encoding:
	compute		op, 4280h or arm6.@dst.value or arm6.@src.value shl 3

finish:	arrange		op, =dw op
	assemble	op
end calminstruction

calminstruction mov? dst, src*
	local		error

	call 		arm6.parse_operand@dst, dst
	call		arm6.parse_operand@src, src

	check		arm6.@dst.type relativeto arm6.register & \
			arm6.@src.type relativeto arm6.register
	jyes		finish
	arrange		error, =err 'destination and source must be registers'
	assemble	error

finish:	local		op
	compute		op, 4600h or arm6.@src.value or \
			    (arm6.@dst.value and 7h) or (arm6.@dst.value and 8h) shl 4
	arrange		op, =dw op
	assemble	op
end calminstruction

calminstruction movs? dst, src*
	local		op, error

	call		arm6.parse_operand@dst, dst
	call		arm6.parse_operand@src, src

	check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
	jyes		cont
	arrange		error, =err 'destination must be low register'
	assemble	error
cont:	check		arm6.@src.type relativeto arm6.immediate
	jyes		imm_encoding
	check		arm6.@src.type relativeto arm6.register
	jyes		reg_encoding
	arrange		error, =err 'source must be register or immediate'
	assemble	error

reg_encoding:
	check		arm6.@src.value > 7
	jno		regfinish
	arrange		error, =err 'source must be low register'
	assemble	error
regfinish:
	compute		op, arm6.@dst.value or arm6.@src.value shl 3
	jump		finish

imm_encoding:
	check		arm6.@src.value < 256
	jyes		immfinish
	arrange		error, =err 'immediate must be below 256'
	assemble	error
immfinish:
	compute		op, 2000h or arm6.@dst.value shl 8 or arm6.@src.value

finish:	arrange		op, =dw op
	assemble	op
end calminstruction

; branching instructions

iterate cond, eq, ne, cs, cc, mi, pl, vc, \
	      hi, ls, ge, lt, gt, le
	calminstruction b#cond? label*
		local		op, address, error

		compute		address, label - ($ + 4)
		check		address and 1
		jno		rangecheck
		arrange		error, =err 'jump is unaligned'
		assemble	error
	rangecheck:
		check		address <= 254 & address >= -256
		jyes		finish
		arrange		error, =err 'jump is too far'
		assemble	error

	finish:	compute		address, address / 2
		compute		op, 0d000h or (% - 1) shl 8 or (address and 0ffh)
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

calminstruction b? label*
	local		op, address, error

	compute		address, label - ($ + 4)
	check		address and 1
	jno		rangecheck
	arrange		error, =err 'jump is unaligned'
	assemble	error
rangecheck:
	check		address <= 2046 & address >= -2048
	jyes		finish
	arrange		error, =err 'jump is too far'
	assemble	error

finish: compute		address, address / 2
	compute		op, 0e000h or (address and 7ffh)
	arrange		op, =dw op
	assemble	op
end calminstruction

macro bal? label*
	b label
end macro

calminstruction bl? label*
	local		op, address, s, j1, j2, error

	compute		address, label - ($ + 4)
	check		address and 1
	jno		rangecheck
	arrange		error, =err 'jump is unaligned'
	assemble	error
rangecheck:
	check		address <= 16777214 & address >= -16777216
	jyes		finish
	arrange		error, =err 'jump is too far'
	assemble	error

finish: compute		address, address / 2
	compute		address, address and 0ffffffh
	compute		s,  (address and 800000h) shr 23
	compute		j1, (address and 400000h) shr 22
	compute		j2, (address and 200000h) shr 21
	compute		j1, not (j1 xor s) and 1h
	compute		j2, not (j2 xor s) and 1h
	compute		op, 0f000h or s shl 10 or (address and 3ff000h) shr 12
	arrange		op, =dw op
	assemble	op
	compute		op, 0d000h or j1 shl 13 or j2 shl 11 or (address and 7ffh)
	arrange		op, =dw op
	assemble	op
end calminstruction

iterate <name, basecode>, \
bx,  4700h,		  \
blx, 4780h
	calminstruction name? label*
		local		op, error

		call		arm6.parse_operand@dst, dst
		check		arm6.@dst.type relativeto arm6.register
		jyes		cont
		arrange		error, =err 'subroutine must be provided through register'
		assemble	error
	cont:	check		arm6.@dst.value <> arm6.regs.pc
		jyes		finish
		arrange		error, =err 'pc not permitted for use in this instruction'
		assemble	error

	finish: compute		op, basecode or arm6.@dst.value shl 3
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

; load/store instructions

calminstruction arm6.parse_ldstm? basereg, wback, highregs, regmask, syntax&
	local		base, writeback, reglist, outmask, error

	match		base=,reglist, syntax
	match		base!, base
	jno		regcont
	compute		writeback, 1
regcont:
	compute		writeback, 0
	publish		wback, writeback

	transform	base, arm6.regs
	jno		regerr
	check		base < 8
	jyes		parse_reglist
regerr:	arrange		error, =err 'base register must be low register'
	assemble	error

parse_reglist:
	publish		basereg, base

	arrange		outmask, =outmask
	call		arm6.parse_reglist, outmask, highregs, reglist
	publish		regmask, outmask
end calminstruction

calminstruction arm6.parse_reglist? outmask, highregs, list*&
	local		reg, tmpmask, more

	match		{list}, list
	jno		generic_reglist_error

	compute		tmpmask, 0
	arrange		list, list=,
parse:	match		reg=,more?, list
	jno		endparse
	arrange		list, more
	transform	reg, arm6.regs
	jno		generic_reglist_error
	check		reg < 8 | highregs
	jyes		cont
	arrange		error, =err 'register list must include low registers only'
	assemble	error
cont:	compute		tmpmask, tmpmask or 1 shl reg
	jump		parse
endparse:
	publish		outmask, tmpmask
	exit

generic_reglist_error:
	arrange		error, =err 'invalid syntax in register list'
	assemble	error
end calminstruction

calminstruction ldm? syntax&
	local		base, wback, highregs, reglist, op, error
	arrange		base,    =base
	arrange		wback,   =wback
	arrange		reglist, =reglist
	compute		highregs, 0

	call		arm6.parse_ldstm, base, wback, highregs, reglist, syntax

	check		wback
	jno		finish
	check		reglist and 1 shl base = 0
	jyes		finish
	arrange		error, =err 'base register cannot have writeback while included in register list'
	assemble	error

finish:	compute		op, 0c800h or reglist or base shl 8
	arrange		op, =dw op
	assemble	op
end calminstruction

calminstruction stm? syntax&
	local		base, wback, highregs, reglist, op, error
	arrange		base, =base
	arrange		wback, =wback
	arrange		reglist, =reglist
	compute		highregs, 0

	call		arm6.parse_ldstm, base, wback, highregs, reglist, syntax

	compute		op, 0c000h or reglist or base shl 8
	arrange		op, =dw op
	assemble	op
end calminstruction

iterate alias, stmia, stmea
	macro alias? syntax&
		stm syntax
	end macro
end iterate

iterate alias, ldmia, ldmfd
	macro alias? syntax&
		ldm syntax
	end macro
end iterate

calminstruction ldr? dst, off*&
	local		op, error

	call		arm6.parse_operand@dst, dst
	call		arm6.parse_operand@src, off

	check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
	jyes		cont
	arrange		error, =err 'destination must be low register'
	assemble	error

cont:	check		arm6.@src.type relativeto arm6.immediate
	jyes		literal

	check		arm6.@src.type relativeto arm6.offset
	jyes		which
	arrange		error, =err 'source must be immediate or offset'
	assemble	error
which:	check		arm6.@src.offtype relativeto arm6.register
	jyes		reg
	check		arm6.@src.offtype relativeto arm6.immediate
	jyes		imm
	arrange		error, =err 'offset must be low register or immediate'
	assemble	error

imm:	check		arm6.@src.offvalue mod 4 = 0
	jyes		immcont
	arrange		error, =err 'offset must be 4 aligned'
	assemble	error
immcont:
	check		arm6.@src.base = arm6.regs.sp
	jyes		immsp
	check		arm6.@src.offvalue <= 124 & arm6.@src.offvalue >= 0
	jyes		immfinish
	arrange		error, =err 'offset must be less than 125'
	assemble	error
immfinish:
	compute		op, 6800h or arm6.@dst.value or arm6.@src.base shl 3 or \
			    (arm6.@src.offvalue shl 6 / 4)
	jump		finish

immsp:	check		arm6.@src.offvalue <= 1020 & arm6.@src.offvalue >= 0
	jyes		immspfinish
	arrange		error, =err 'offset must be less than 1021'
	assemble	error
immspfinish:
	compute		op, 9800h or arm6.@dst.value shl 8 or arm6.@src.value
	jump		finish

reg:	compute		op, 5800h or arm6.@dst.value or arm6.@src.base shl 3 or \
			    arm6.@src.offvalue shl 6
	jump		finish

literal:
	check		arm6.@src.value mod 4 = 0
	jyes		rangecheck
	arrange		error, =err 'label must be 4 aligned'
	assemble	error
rangecheck:
	compute		arm6.@src.value, arm6.@src.value - ($ + 4)
	check		arm6.@src.value <= 1020 & arm6.@src.value >= 0
	jyes		litfinish
	arrange		error, =err 'label is too far'
	assemble	error

litfinish:
	compute		op, 4800h or arm6.@dst.value shl 8 or (arm6.@src.value / 4)

finish: arrange		op, =dw op
	assemble	op
end calminstruction

calminstruction str? dst, off*&
	local		op, error

	call		arm6.parse_operand@dst, dst
	call		arm6.parse_operand@src, off

	check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
	jyes		cont
	arrange		error, =err 'destination must be low register'
	assemble	error

cont:	check		arm6.@src.type relativeto arm6.offset
	jyes		which
	arrange		error, =err 'source must be immediate or offset'
	assemble	error
which:	check		arm6.@src.offtype relativeto arm6.register
	jyes		reg
	check		arm6.@src.offtype relativeto arm6.immediate
	jyes		imm
	arrange		error, =err 'offset must be low register or immediate'
	assemble	error

imm:	check		arm6.@src.offvalue mod 4 = 0
	jyes		immcont
	arrange		error, =err 'offset must be 4 aligned'
	assemble	error
immcont:
	check		arm6.@src.base = arm6.regs.sp
	jyes		immsp
	check		arm6.@src.offvalue <= 124 & arm6.@src.offvalue >= 0
	jyes		immfinish
	arrange		error, =err 'offset must be less than 125'
	assemble	error
immfinish:
	compute		op, 6000h or arm6.@dst.value or arm6.@src.base shl 3 or \
			    (arm6.@src.offvalue shl 6 / 4)
	jump		finish

immsp:	check		arm6.@src.offvalue <= 1020 & arm6.@src.offvalue >= 0
	jyes		immspfinish
	arrange		error, =err 'offset must be less than 1021'
	assemble	error
immspfinish:
	compute		op, 9000h or arm6.@dst.value shl 8 or arm6.@src.value
	jump		finish

reg:	compute		op, 5000h or arm6.@dst.value or arm6.@src.base shl 3 or \
			    arm6.@src.offvalue shl 6

finish: arrange		op, =dw op
	assemble	op
end calminstruction

iterate <name, regcode, immcode>, \
ldrb, 5c00h, 7800h, 		  \
strb, 5400h, 7000h
	calminstruction name? dst, src*&
		local		op, error

		call		arm6.parse_operand@dst, dst
		call		arm6.parse_operand@src, src

		check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
		jyes		cont
		arrange		error, =err 'destination must be low register'
		assemble	error
	cont:	check		arm6.@src.type relativeto arm6.offset
		jyes		which
		arrange		error, =err 'source must be offset'
		assemble	error

	which:	check		arm6.@src.offtype relativeto arm6.register
		jyes		reg
		check		arm6.@src.offtype relativeto arm6.immediate
		jyes		imm
		arrange		error, =err 'offset must be low register or immediate'
		assemble	error

	imm:	check		arm6.@src.offvalue <= 31 & arm6.@src.offvalue >= 0
		jyes		immfinish
		arrange		error, =err 'offset must be below 32'
		assemble	error
	immfinish:
		compute		op, immcode or arm6.@src.value or arm6.@src.base shl 3 or \
				    arm6.@src.offvalue shl 6
		jump		finish

	reg:	compute		op, regcode or arm6.@dst.value or arm6.@src.base shl 3 or \
				    arm6.@src.offvalue shl 6

	finish:	arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

iterate <name, regcode, immcode>, \
ldrh, 5a00h, 8800h,		  \
strh, 5200h, 8000h
	calminstruction name? dst, src*&
		local		op, error

		call		arm6.parse_operand@dst, dst
		call		arm6.parse_operand@src, src

		check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
		jyes		cont
		arrange		error, =err 'destination must be low register'
		assemble	error
	cont:	check		arm6.@src.type relativeto arm6.offset
		jyes		which
		arrange		error, =err 'source must be offset'
		assemble	error

	which:	check		arm6.@src.offtype relativeto arm6.register
		jyes		reg
		check		arm6.@src.offtype relativeto arm6.immediate
		jyes		imm
		arrange		error, =err 'offset must be low register or immediate'
		assemble	error

	imm:	check		arm6.@src.offvalue mod 2 = 0
		jyes		immrange
		arrange		error, =err 'offset must be 2 aligned'
		assemble	error
	immrange:
		check		arm6.@src.offvalue <= 62 & arm6.@src.offvalue >= 0
		jyes		immfinish
		arrange		error, =err 'offset must be below 63'
		assemble	error
	immfinish:
		compute		op, immcode or arm6.@src.value or arm6.@src.base shl 3 or \
				    arm6.@src.offvalue shl 6
		jump		finish

	reg:	compute		op, regcode or arm6.@dst.value or arm6.@src.base shl 3 or \
				    arm6.@src.offvalue shl 6

	finish:	arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

iterate <name, basecode>, \
ldrsb, 5600h,		  \
ldrsh, 5e00h
	calminstruction name? dst, src*&
		local		op, error

		call		arm6.parse_operand@dst, dst
		call		arm6.parse_operand@src, src

		check		arm6.@dst.type relativeto arm6.register & arm6.@dst.value < 8
		jyes		cont
		arrange		error, =err 'destination must be low register'
		assemble	error
	cont:	check		arm6.@src.type relativeto arm6.offset
		jyes		cont2
		arrange		error, =err 'source must be offset'
		assemble	error
	cont2:	check		arm6.@src.offtype relativeto arm6.register 
		jyes		finish
		arrange		error, =err 'offset must be register'
		assemble	error

	finish:	compute		op, basecode or arm6.@dst.value or arm6.@src.base shl 3 or \
				    arm6.@src.offvalue shl 6
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

; hint/misc instructions

iterate <name, opcode>, \
nop,   0bf00h, 		\
sev,   0bf40h, 		\
wfe,   0bf20h, 		\
wfi,   0bf30h, 		\
yield, 0bf10h
	calminstruction name?
		local		op
		arrange		op, =dw opcode
		assemble	op
	end calminstruction
end iterate

iterate name, ie, id
	calminstruction cps#name? i*
		local		op, error

		match		=i?, i
		jyes		finish
		arrange		error, =err 'cps requires i flag'
		assemble	error

	finish:	compute		op, 0b662h or (% - 1) shl 4
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

calminstruction msr? sys, dst*
	local		op, error

	call		arm6.parse_operand@dst, dst
	check		arm6.@dst.type relativeto arm6.register
	jyes		cont
	arrange		error, =err 'destination must be register'
	assemble	error
cont:	check		arm6.@dst.value <> arm6.regs.sp & arm6.@dst.value <> arm6.regs.pc
	jyes		cont2
	arrange		error, =err 'pc or sp not permitted for use in this instruction'
	assemble	error

cont2:	transform	sys, arm6.sysregs
	jyes		finish
	arrange		error, =err 'unknown system register'
	assemble	error

finish:	compute		op, 0f380h or arm6.@dst.value
	arrange		op, =dw op
	assemble	op
	compute		op, 8800h or sys
	arrange		op, =dw op
	assemble	op
end calminstruction

calminstruction mrs? dst, sys*
	local		op, error

	call		arm6.parse_operand@dst, dst
	check		arm6.@dst.type relativeto arm6.register
	jyes		cont
	arrange		error, =err 'destination must be register'
	assemble	error
cont:	check		arm6.@dst.value <> arm6.regs.sp & arm6.@dst.value <> arm6.regs.pc
	jyes		cont2
	arrange		error, =err 'pc or sp not permitted for use in this instruction'
	assemble	error

cont2:	transform	sys, arm6.sysregs
	jyes		finish
	arrange		error, =err 'unknown system register'
	assemble	error

finish:	arrange		op, =dw 0f3efh
	assemble	op
	compute		op, 8000h or sys or arm6.@dst.value shl 8
	arrange		op, =dw op
	assemble	op
end calminstruction

iterate <name, basecode>, \
	udf,  0de00h,	  \
	svc,  0df00h,	  \
	bkpt, 0be00h
	calminstruction name? imm:0
		local		op, error

		call		arm6.parse_operand@aux, imm

		check		arm6.@aux.value < 256
		jyes		finish
		arrange		error, =err 'immediate must be less than 256'
		assemble	error

	finish:	compute 	op, basecode or arm6.@aux.value
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate

define udf?
calminstruction udf.w? imm:0
	local 		op

	call		arm6.parse_operand@aux, imm

	check		arm6.@aux.value < 65536
	jyes		finish
	arrange		error, =err 'immediate must be less than 65536'
	assemble	error

finish:	compute 	op, 0f7f0h or (arm6.@aux.value and 0f000h) shr 12
	arrange		op, =dw op
	assemble	op
	compute		op, 0a000h or (arm6.@aux.value and 0fffh)
	arrange		op, =dw op
	assemble	op
end calminstruction

iterate <name, code1, code2>, \
dsb, 0f3bfh, 8f4fh,	      \
dmb, 0f3bfh, 8fafh,	      \
isb, 0f3bfh, 8f6fh
	calminstruction name? sy:
		local		op, error

		match		, sy
		jyes		skip
		match		=sy?, sy
		jyes		skip
		arrange		error, =err 'only permitted options are sy'
		assemble	error

	skip:	arrange		op, =dw code1
		assemble	op
		arrange		op, =dw code2
		assemble	op
	end calminstruction
end iterate

iterate <name, specialreg, specialmask, opcode>, \
push, arm6.regs.lr, 0bfffh, 0b400h,	    	 \
pop,  arm6.regs.pc, 07fffh, 0bc00h
	calminstruction name? list*&
		local 		listmask, highregs, special, op, error
		arrange		listmask, =listmask
		compute		highregs, 1

		call		arm6.parse_reglist, listmask, highregs, list
		compute		special, ((1 shl specialreg) and listmask) shr specialreg
		compute		listmask, specialmask and listmask
		check		listmask > 0ffh
		jno		finish
		arrange		error, =err 'only permitted high register is lr'
		assemble	error

	finish:	compute		op, opcode or special shl 8 or listmask
		arrange		op, =dw op
		assemble	op
	end calminstruction
end iterate
