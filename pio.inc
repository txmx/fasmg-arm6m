define pio?
namespace pio
	optset = 0
	sideset = 0
	cursideset = 0

	calminstruction parse_sidedelay? sdmask, line&
		local curside
		arrange curside, =pio.=cursideset		

		local maskval, sideval, delayval
		compute maskval,  0
		compute sideval,  0
		compute delayval, 0

		match , line
		jyes sidestick

		match =side sideval [delayval], line
		jno sideonly
		compute maskval, (1 and optset) shl 12
		compute sideval, sideval
		compute delayval, delayval
		jump delaycheck
	sideonly:
		match =side sideval, line
		jno delayonly
		compute maskval, (1 and optset) shl 12
		compute sideval, sideval
		compute delayval, 0
		jump sidecheck
	delayonly:
		match [delayval], line
		jno errmatch
		compute maskval, 0
		compute delayval, delayval
		compute sideval, 0
	sidestick:
		check optset
		jyes delaycheck
		compute sideval, cursideset

	delaycheck:
		check delayval > 1fh shr sideset
		jno sidecheck
		arrange error, =err 'delay too large for sideset'
		assemble error
	sidecheck:
		check sideval > 1 shl sideset - 1
		jno finish
		arrange error, =err 'no side bits available'
		assemble error

	finish:	compute maskval, maskval or delayval shl 8 or \
				 sideval shl (8 + (5 - (sideset + optset)))
		publish sdmask, maskval
		publish :curside, sideval
		exit

	errmatch:
		arrange error, =err 'unknown syntax encountered at end of instruction'
		assemble error
	end calminstruction

	calminstruction jmp? syntax&
		local 	condmask, address, more, error

		match 	!=x? more, syntax
		compute condmask, 001'00000b
		jyes 	parseaddr
		match 	=x?-- more, syntax
		compute condmask, 010'00000b
		jyes 	parseaddr
		match 	!=y? more, syntax
		compute condmask, 011'00000b
		jyes 	parseaddr
		match 	=y?-- more, syntax
		compute condmask, 100'00000b
		jyes 	parseaddr
		match 	=x?!===y? more, syntax
		compute condmask, 101'00000b
		jyes 	parseaddr
		match 	=pin? more, syntax
		compute condmask, 110'00000b
		jyes 	parseaddr
		match 	!=osre? more, syntax
		compute condmask, 111'00000b
		jyes 	parseaddr
		compute condmask, 0
		match 	address more?, syntax
		jump 	checkaddr

	parseaddr:
		match 	address more?, more
	checkaddr:
		;transform address
		compute address, address / 2
		check 	address < 32
		jyes 	sidedelay
		arrange error, =err 'address out of range'
		assemble error
	sidedelay:
		local 	sdmask
		arrange sdmask, =sdmask
		call 	parse_sidedelay, sdmask, more

	finish:	local 	op
		compute op, sdmask or condmask or address
		arrange op, =dw op
		assemble op
	end calminstruction

	calminstruction wait? syntax&
		local 	polarity, type, index, rel, more, error
		match 	polarity type index more?, syntax
		compute rel, 0

		compute polarity, polarity
		check 	polarity > 1
		compute polarity, polarity shl 7
		jno 	parse
		arrange error, =err 'polarity can be either 1 or 0'
		assemble error

	parse:  transform type, wait
		check 	type = wait.irq
		jno 	noirq
		check 	index <= 7
		jyes 	relcheck
		arrange error, =err 'index can only be 0 to 7'
		assemble error
	relcheck:
		match 	=rel? more?, more
		compute rel, 10000b
		jump 	sidedelay
	noirq:  check 	index < 32
		jyes 	sidedelay
		arrange error, =err 'index can only be below 32'
		assemble error

	sidedelay:
		local 	sdmask
		arrange sdmask, =sdmask
		call	parse_sidedelay, sdmask, more

	finish:	local 	op
		compute op, 001b shl 13 or polarity or \
			    type or index or rel or sdmask
		arrange op, =dw op
		assemble op
	end calminstruction

	define wait?
	iterate type, gpio, pin, irq
		wait.type equ (% - 1) shl 5
	end iterate

	iterate instr, in, out
		calminstruction instr? syntax&
			local 	dst, bitcount, more, error

			match 	dst=,bitcount more?, syntax
			jyes 	checkbits
			arrange error, =err 'invalid syntax'
			assemble error

		checkbits:
			check	bitcount > 0 & bitcount <= 32
			jyes 	parse
			arrange error, =err 'bitcount can only be 1 to 32'
			assemble error

		parse:	compute bitcount, bitcount and 1fh
			transform dst, instr
			jyes 	sidedelay
			arrange error, =err 'unknown operand'
			assemble error

		sidedelay:
			local 	sdmask
			arrange sdmask, =sdmask
			call 	parse_sidedelay, sdmask, more

		finish:	local	op
			compute op, (% + 1) shl 13 or sdmask \
				    or bitcount or dst
			arrange op, =dw op
			assemble op
		end calminstruction
	end iterate

	define in?
	iterate source, pins, x, y, null, reserved, reserved, isr, osr
		in.source equ (% - 1) shl 5
	end iterate

	define out?
	iterate destination, pins, x, y, null, pindirs, pc, isr, exec
		out.destination equ (% - 1) shl 5
	end iterate

	iterate <instr, type, special>, push, 0, iffull, pull, 1, ifempty
		calminstruction instr? syntax&
			local 	more, error, sdmask, extra, blocking			
			
			match 	, syntax
			compute sdmask, 0
			compute extra, 0
			compute blocking, 1
			jyes 	finish

			match 	=special? more?, syntax
			jyes 	special
			match 	more, syntax
			jump 	blocking
		special:
			compute extra, 1
		blocking:
			match 	=block? more?, more
			jyes 	sidedelay
			match	 =noblock? more?, more
			jyes 	noblock
			jump 	sidedelay
		noblock:
			compute blocking, 0

		sidedelay:
			arrange sdmask, =sdmask
			call 	parse_sidedelay, sdmask, more

		finish: local 	op
			compute op, 100b shl 13 or type shl 7 \
				    or extra shl 6 or blocking shl 5 \
				    or sdmask
			arrange op, =dw op
			assemble op
		end calminstruction
	end iterate

	calminstruction mov? syntax&
		local 	src, dst, vop, more, error
		
		match 	dst =, :: src more?, syntax
		compute vop, mov.operation.reverse
		jyes 	parse
		match 	dst =, ~  src more?, syntax
		compute vop, mov.operation.negate
		jyes 	parse
		match 	dst =, !  src more?, syntax
		compute vop, mov.operation.negate
		jyes 	parse

		compute vop, 0
		match 	dst =, src more?, syntax
	parse:  transform src, mov.source
		jyes 	cont
		arrange error, =err 'invalid source operand'
		assemble error
	cont:	transform dst, mov.destination
		jyes 	sidedelay
		arrange error, =err 'invalid destination operand'
		assemble error
	
	sidedelay:
		local 	sdmask
		arrange sdmask, =sdmask
		call 	parse_sidedelay, sdmask, more
	
	finish: local 	op
		compute op, 101b shl 13 or dst or \
			    vop or src or sdmask
		arrange op, =dw op
		assemble op
	end calminstruction

	define mov?

	iterate src, pins, x, y, null, reserved, status, isr, osr
		mov.source.src equ (% - 1)
	end iterate

	iterate dst, pins, x, y, reserved, exec, pc, isr, osr
		mov.destination.dst equ (% - 1) shl 5
	end iterate

	iterate op, negate, reverse
		mov.operation.op equ % shl 3
	end iterate

	calminstruction irq? syntax&
		local 	clear, wait, index, more, error

		match 	, syntax
		compute clear, 0
		compute wait, 0
		compute index, 0
		jyes 	finish

		match 	=set? more, syntax
		jyes 	parse
		match 	=nowait? more, syntax
		jyes 	parse
		match 	=wait? more, syntax
		compute wait, 1
		jyes 	parse
		match 	=clear? more, syntax
		compute clear, 1
		jyes 	parse

		compute wait, 0
		compute clear, 0
		arrange more, syntax
	parse:	match 	index more?, more
		check 	index > 7
		jno 	relcheck
		arrange error, =err 'index can only be 0 to 7'
		assemble error

	relcheck:
		match 	=rel? more?, more
		jno 	sidedelay
		compute index, index or 10h

	sidedelay:
		local 	sdmask
		arrange sdmask, =sdmask
		call 	parse_sidedelay, sdmask, more

	finish:	local 	op
		compute op, 110b shl 13 or sdmask or \
			    clear shl 6 or wait shl 5 or index
		arrange op, =dw op
		assemble op
	end calminstruction

	calminstruction set? syntax&
		local 	dst, index, more, error

		match 	dst =, index more?, syntax
		check 	index < 32
		jyes 	parse
		arrange error, =err 'index must be below 32'
		assemble error

	parse:	transform dst, set
		jyes 	sidedelay
		arrange error, =err 'invalid destination operand'
		assemble error

	sidedelay:
		local 	sdmask
		arrange sdmask, =sdmask
		call 	parse_sidedelay, sdmask, more

	finish: local 	op
		compute op, 111b shl 13 or sdmask or dst shl 5 or index
		arrange op, =dw op
		assemble op
	end calminstruction

	define set?
	iterate destination, pins, x, y, reserved, pindirs
		set.destination equ (% - 1) shl 5
	end iterate

	calminstruction nop? sidedelay&
		local 	sdmask
		arrange sdmask, =sdmask
		call 	parse_sidedelay, sdmask, sidedelay

		local 	op
		compute op, 101'00000'010'00'010b or sdmask
		arrange op, =dw op
		assemble op
	end calminstruction
end namespace

macro program? name*, options&
	local value, opt
	rawmatch =sideset? value =opt?, options
		name.sideset := value
		name.optset  := 1
	else rawmatch =sideset? value, options
		name.sideset := value
		name.optset  := 0
	else
		name.sideset := 0
		name.optset  := 0
	end rawmatch
	if name.sideset + name.optset > 5
		err 'sideset too large'
	end if
	pio.optset  = name.optset
	pio.sideset = name.sideset

	virtual at 0
	name::

	namespace name

	iterate instr, jmp, wait, in, out, push, \
		       pull, mov, irq, set, nop
		mvmacro instr, pio.instr
	end iterate

	macro wrap? target
		match =target, target
			name.wraptarget := $
		else match , target
			name.wrap := $
		end match
	end macro
end macro

macro end?.program?
	purge wrap
	iterate instr, jmp, wait, in, out, push, \
		       pull, mov, irq, set, nop
		mvmacro pio.instr, instr
	end iterate
	programsize := $ - $$

	end namespace
	end virtual
end macro

macro emitprogram? name*
	local program
	load program:name.programsize from name:0
	db program
end macro

