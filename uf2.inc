define uf2?
namespace uf2?
	start_magic  equ 5546320ah
	start_magic2 equ 57515d9eh
	end_magic    equ 306fb10ah

	flag_notmain   := 0001h
	flag_container := 1000h
	flag_familyid  := 2000h
	flag_md5       := 4000h
	flag_ext       := 8000h

	flags	    = 0
	targetaddr  = 0
	payloadsize = 0
	familysize  = 0

	define family?
	namespace family?
		rp2040? equ $e48bff65
	end namespace
end namespace

macro format?.uf2? options&
	calminstruction	parse_options? opts&
		local		option, value

	loop:	match		option value opts?, opts
		jno		cont

		match		=family?, option
		jno		addrf
		transform	value, uf2.family
		compute		uf2.familysize, value
		compute		uf2.flags, uf2.flags or uf2.flag_familyid
		jump		cont

	addrf:	match		=address?, option
		jno		bsize
		compute		uf2.targetaddr, value
		jump		cont

	bsize:	match		=page?, option
		jno		cont
		compute		uf2.payloadsize, (476 / value) * value

	cont:	match		=, opts, opts
		jyes		loop
	end calminstruction

	local data, blocks, remains

	parse_options options

	virtual at 0
	uf2.usergenerated::

	postpone ?
		blocks = $ / uf2.payloadsize
		remains = $ mod uf2.payloadsize
		if remains > 0
			blocks = blocks + 1
			db (uf2.payloadsize - remains) dup 0
		end if
		end virtual

		repeat blocks, block:0
			dd uf2.start_magic
			dd uf2.start_magic2

			dd uf2.flags
			dd uf2.targetaddr
			dd uf2.payloadsize
			dd block
			dd blocks
			dd uf2.familysize

			load data:uf2.payloadsize \
			from uf2.usergenerated:(block * uf2.payloadsize)
			db data
			db (476 - uf2.payloadsize) dup 0

			dd uf2.end_magic

			uf2.targetaddr = uf2.targetaddr + uf2.payloadsize
		end repeat
	end postpone
end macro
