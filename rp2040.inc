include 'arm6.inc'
include 'pio.inc'
include 'uf2.inc'

targetaddr_flash := $10000000
targetaddr_ram   := $20000000
targetaddr_cache := $15000000

namespace uf2?
	targetaddr  = targetaddr_flash
	familysize  = family.rp2040
	payloadsize = 256
	flags = flags or flag_familyid
end namespace
