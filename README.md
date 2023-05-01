# fasmg ARM6-M
This is exclusively an implementation of the ARM6-M instruction set for [fasmg](https://flatassembler.net/docs.php?article=fasmg_manual). I made this for use specifically with the RP2040 chip, which contains a Cortex-M0+ processor.
The document used to create this was the ARMv6-M Architecture Reference Manual (DDI 0419E)

## Notes about some syntax changes
### 1. PIO
For PIO, the syntax is much stricter than the default implementation. Unlike the default assembler, this one doesn't allow liberal usage of commas between certain arguments for instructions. A feature provided, however, is sticky sideset, which uses the previously enabled side when omitted. It should be noted that while this can hopefully save an optional bit on occasion, it does not carry the same behavior.
### 2. ARM
Deprecated syntaxes were left unimplemented; deprecated encodings, however, were kept.
