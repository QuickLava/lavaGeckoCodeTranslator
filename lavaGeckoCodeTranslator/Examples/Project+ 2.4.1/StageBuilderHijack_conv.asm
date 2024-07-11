# Note: From "Source/Project+/"
# Good example of some *very* Gecko-y code, provides examples of some more
# less frequently used Gecko stuff (namely Gecko Registers and Loops)

Stage Roster Expansion System v2.0 + Stage Builder Redirector v2.1 [Codes, Phantom Wings, Desi]
* E0000000 80008000             # Full Terminator: ba = 0x80000000, po = 0x80000000
* 04043B20 380000FF             # 32-Bit Write @ $(ba + 0x00043B20):  0x380000FF
* 04043B40 1C060018             # 32-Bit Write @ $(ba + 0x00043B40):  0x1C060018
* 04043B58 38840018             # 32-Bit Write @ $(ba + 0x00043B58):  0x38840018
* 04949C8C 3884B460             # 32-Bit Write @ $(ba + 0x00949C8C):  0x3884B460
* 04949D10 3884B460             # 32-Bit Write @ $(ba + 0x00949D10):  0x3884B460
* 04949E20 3884B460             # 32-Bit Write @ $(ba + 0x00949E20):  0x3884B460
* 04949EFC 3884B460             # 32-Bit Write @ $(ba + 0x00949EFC):  0x3884B460
HOOK @ $8094A588                # Address = $(ba + 0x0094A588) [in "entryEntity/[stLoaderStage]/st_loader_stage.o" @ $8094A530]
{
	lhz r3, 0x1a(r3)                # 0xA063001A
	cmpwi r3, 0x40                  # 0x2C030040
	blt loc_0x004                   # 0x41800008
	li r3, 0x0                      # 0x38600000
loc_0x004:
	nop                             # 0x60000000
}
HOOK @ $8094A1D0                # Address = $(ba + 0x0094A1D0) [in "processBegin/[stLoaderStage]/st_loader_stage.o" @ $80949FD4]
{
	mr r29, r3                      # 0x7C7D1B78
	cmpwi r3, -0x1                  # 0x2C03FFFF
	bne loc_0x004                   # 0x40820008
	lis r29, 0x3f                   # 0x3FA0003F
loc_0x004:
	nop                             # 0x60000000
}
* 04015564 48000010             # 32-Bit Write @ $(ba + 0x00015564):  0x48000010
* 2042AEB0 00000041             # 32-Bit If Equal: If Val @ $(ba + 0x0042AEB0) == 0x00000041
* 0442AEB0 00000000             # 32-Bit Write @ $(ba + 0x0042AEB0):  0x00000000
* 80000000 8042AEB0             # Set Gecko Register: gr0 = 0x8042AEB0
* 80000001 8042AEB0             # Set Gecko Register: gr1 = 0x8042AEB0
* 60000030 00000000             # Set Repeat: Repeat 48 times, Store in b0
* 8A001801 00000000             # 	Memory Copy 1: Copy 0x18 byte(s) from $(gr0) to $(gr1)
* 86000000 00000024             # 	Gecko Reg Arith (Immediate): gr0 = gr0 + 0x00000024
* 86000001 00000018             # 	Gecko Reg Arith (Immediate): gr1 = gr1 + 0x00000018
* 62000000 00000000             # Execute Repeat: Execute Repeat in b0
* 0042B348 024B0000             # 8-Bit Write @ $(ba + 0x0042B348):  0x00 (588 times)
* 0442B348 00000040             # 32-Bit Write @ $(ba + 0x0042B348):  0x00000040
* 0642B34C 00000010             # String Write (16 characters, 2 strings) @ $(ba + 0x0042B34C):
* 73745F63 7573746F             # 	   "st_custo ...
* 6D00002E 72656C00             # 	... m"".rel"
* 80000000 8042BC18             # Set Gecko Register: gr0 = 0x8042BC18
* 80000001 8042B460             # Set Gecko Register: gr1 = 0x8042B460
* 8A010001 00000000             # Memory Copy 1: Copy 0x100 byte(s) from $(gr0) to $(gr1)
* 0442B560 8042BC14             # 32-Bit Write @ $(ba + 0x0042B560):  0x8042BC14
* 0042BC18 010F0000             # 8-Bit Write @ $(ba + 0x0042BC18):  0x00 (272 times)
* 0642BC14 00000008             # String Write (8 characters) @ $(ba + 0x0042BC14):
* 43757374 6F6D0000             # 	"Custom"
* E0000000 80008000             # Full Terminator: ba = 0x80000000, po = 0x80000000
* 04043B34 483E783C             # 32-Bit Write @ $(ba + 0x00043B34):  0x483E783C
* 0642B370 00000060             # String Write (96 characters) @ $(ba + 0x0042B370):
* 2C060031 4080000C             # 	0x2C, 0x06, 0x00, 0x31, 0x40, 0x80, 0x00, 0x0C
* 80040000 48000050             # 	0x80, 0x04, 0x00, 0x00, 0x48, 0x00, 0x00, 0x50
* 3806000F 7C030000             # 	0x38, 0x06, 0x00, 0x0F, 0x7C, 0x03, 0x00, 0x00
* 40820040 3866FFD0             # 	0x40, 0x82, 0x00, 0x40, 0x38, 0x66, 0xFF, 0xD0
* 2C0000FE 4082000C             # 	0x2C, 0x00, 0x00, 0xFE, 0x40, 0x82, 0x00, 0x0C
* 38C40024 48000008             # 	0x38, 0xC4, 0x00, 0x24, 0x48, 0x00, 0x00, 0x08
* 38C4000F 38860001             # 	0x38, 0xC4, 0x00, 0x0F, 0x38, 0x86, 0x00, 0x01
* 3CA08042 60A5B450             # 	0x3C, 0xA0, 0x80, 0x42, 0x60, 0xA5, 0xB4, 0x50
* 38E00000 7E6802A6             # 	0x38, 0xE0, 0x00, 0x00, 0x7E, 0x68, 0x02, 0xA6
* 4BFCB801 7E6803A6             # 	0x4B, 0xFC, 0xB8, 0x01, 0x7E, 0x68, 0x03, 0xA6
* 38C00031 4BC1877C             # 	0x38, 0xC0, 0x00, 0x31, 0x4B, 0xC1, 0x87, 0x7C
* 3884FFE8 4BC1876C             # 	0x38, 0x84, 0xFF, 0xE8, 0x4B, 0xC1, 0x87, 0x6C
* 04949C88 4BAE1749             # 32-Bit Write @ $(ba + 0x00949C88):  0x4BAE1749
* 04949D0C 4BAE16C5             # 32-Bit Write @ $(ba + 0x00949D0C):  0x4BAE16C5
* 04949E1C 4BAE15B5             # 32-Bit Write @ $(ba + 0x00949E1C):  0x4BAE15B5
* 04949F08 4BAE14C9             # 32-Bit Write @ $(ba + 0x00949F08):  0x4BAE14C9
* 0642B3D0 00000070             # String Write (112 characters) @ $(ba + 0x0042B3D0):
* 9421FF80 BC610010             # 	0x94, 0x21, 0xFF, 0x80, 0xBC, 0x61, 0x00, 0x10
* 7C6802A6 9061000C             # 	0x7C, 0x68, 0x02, 0xA6, 0x90, 0x61, 0x00, 0x0C
* 2C000040 41800040             # 	0x2C, 0x00, 0x00, 0x40, 0x41, 0x80, 0x00, 0x40
* 7C030378 3863FFC1             # 	0x7C, 0x03, 0x03, 0x78, 0x38, 0x63, 0xFF, 0xC1
* 3CC08042 2C0300BF             # 	0x3C, 0xC0, 0x80, 0x42, 0x2C, 0x03, 0x00, 0xBF
* 4082000C 60C6BC34             # 	0x40, 0x82, 0x00, 0x0C, 0x60, 0xC6, 0xBC, 0x34
* 48000008 60C6BC1C             # 	0x48, 0x00, 0x00, 0x08, 0x60, 0xC6, 0xBC, 0x1C
* 38860001 3CA08042             # 	0x38, 0x86, 0x00, 0x01, 0x3C, 0xA0, 0x80, 0x42
* 60A5B450 38E00000             # 	0x60, 0xA5, 0xB4, 0x50, 0x38, 0xE0, 0x00, 0x00
* 39200000 4BFCB79D             # 	0x39, 0x20, 0x00, 0x00, 0x4B, 0xFC, 0xB7, 0x9D
* 38000040 540013BA             # 	0x38, 0x00, 0x00, 0x40, 0x54, 0x00, 0x13, 0xBA
* 8061000C 7C6803A6             # 	0x80, 0x61, 0x00, 0x0C, 0x7C, 0x68, 0x03, 0xA6
* B8610010 80210000             # 	0xB8, 0x61, 0x00, 0x10, 0x80, 0x21, 0x00, 0x00
* 4E800020 00000000             # 	0x4E, 0x80, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00
* 0042B455 00000078             # 8-Bit Write @ $(ba + 0x0042B455):  0x78
* 0442B45C 00000002             # 32-Bit Write @ $(ba + 0x0042B45C):  0x00000002
* 285A7C16 00007365             # 16-Bit If Equal: If Val @ $(ba + 0x005A7C16) == 0x7365
* 285A7C18 00006C6D             # 16-Bit If Equal: If Val @ $(ba + 0x005A7C18) == 0x6C6D
* 285A7C1A 00006170             # 16-Bit If Equal: If Val @ $(ba + 0x005A7C1A) == 0x6170
* 2042BC2C 00000001             # 32-Bit If Equal: If Val @ $(ba + 0x0042BC2C) == 0x00000001
* 0442BC2C 00000000             # 32-Bit Write @ $(ba + 0x0042BC2C):  0x00000000
* 0242BC14 000B0000             # 16-Bit Write @ $(ba + 0x0042BC14):  0x0000 (12 times)
* 0242B34F 000B0000             # 16-Bit Write @ $(ba + 0x0042B34F):  0x0000 (12 times)
* 0642B34C 00000010             # String Write (16 characters, 2 strings) @ $(ba + 0x0042B34C):
* 73745F63 7573746F             # 	   "st_custo ...
* 6D00002E 72656C00             # 	... m"".rel"
* 0642BC14 00000008             # String Write (8 characters) @ $(ba + 0x0042BC14):
* 43757374 6F6D0000             # 	"Custom"
* E0000000 80008000             # Full Terminator: ba = 0x80000000, po = 0x80000000
* 2A708CEB 000000FE             # 16-Bit If Not Equal (With Endif): If Val @ $(ba + 0x00708CEA) != 0x00FE
* 2042BC2C 00000001             # 32-Bit If Equal: If Val @ $(ba + 0x0042BC2C) == 0x00000001
* 0442BC2C 00000000             # 32-Bit Write @ $(ba + 0x0042BC2C):  0x00000000
* 0242BC14 000B0000             # 16-Bit Write @ $(ba + 0x0042BC14):  0x0000 (12 times)
* 0242B34F 000B0000             # 16-Bit Write @ $(ba + 0x0042B34F):  0x0000 (12 times)
* 0642B34C 00000010             # String Write (16 characters, 2 strings) @ $(ba + 0x0042B34C):
* 73745F63 7573746F             # 	   "st_custo ...
* 6D00002E 72656C00             # 	... m"".rel"
* 0642BC14 00000008             # String Write (8 characters) @ $(ba + 0x0042BC14):
* 43757374 6F6D0000             # 	"Custom"
* E0000000 80008000             # Full Terminator: ba = 0x80000000, po = 0x80000000
* 28708CEB 00000035             # 16-Bit If Equal (With Endif): If Val @ $(ba + 0x00708CEA) == 0x0035
* 4A000000 90000000             # Set Pointer Offset: po = 0x90000000
* 3816B55C 0000002D             # 16-Bit If Equal: If Val @ $(po + 0x0016B55C) == 0x002D
* 10180F3B 000000FE             # 8-Bit Write @ $(po + 0x00180F3B):  0xFE
* 0442BC24 9016B55F             # 32-Bit Write @ $(ba + 0x0042BC24):  0x9016B55F
* 0442BC28 8042B34F             # 32-Bit Write @ $(ba + 0x0042BC28):  0x8042B34F
* 80000000 9016B55F             # Set Gecko Register: gr0 = 0x9016B55F
* 80000001 8042BC24             # Set Gecko Register: gr1 = 0x8042BC24
* 80000002 8042B34F             # Set Gecko Register: gr2 = 0x8042B34F
* 80000003 8042BC28             # Set Gecko Register: gr3 = 0x8042BC28
* 6000000F 00000000             # Set Repeat: Repeat 15 times, Store in b0
* 48001001 00000000             # 	Load Pointer Offset: po = Val @ $(gr1 + 0x00000000)
* 38000000 00000000             # 	16-Bit If Equal: If Val @ $(po + 0x00000000) == 0x0000
* 48001003 00000000             # 	Load Pointer Offset: po = Val @ $(gr3 + 0x00000000)
* 16000000 00000008             # 	String Write (8 characters) @ $(po + 0x00000000):
* 2E72656C 00000000             # 		".rel"
* 0442BC2C 00000002             # 	32-Bit Write @ $(ba + 0x0042BC2C):  0x00000002
* 04708CEB 000000FE             # 	32-Bit Write @ $(ba + 0x00708CEB):  0x000000FE
* 60000007 00000000             # 	Set Repeat: Repeat 7 times, Store in b0
* E2100000 00000000             # 		Endif (Invert Exec Status): Apply 0 Endifs, ba unchanged, po unchanged
* 8A000202 00000000             # 		Memory Copy 1: Copy 0x2 byte(s) from $(gr0) to $(gr2)
* 8A000202 000008C5             # 		Memory Copy 1: Copy 0x2 byte(s) from $(gr0) to $(gr2 + 0x000008C5)
* 86000000 00000002             # 		Gecko Reg Arith (Immediate): gr0 = gr0 + 0x00000002
* 86010001 00000002             # 		Gecko Reg Arith (Immediate): Val @ $(gr1) = Val @ $(gr1) + 0x00000002
* 86000002 00000001             # 		Gecko Reg Arith (Immediate): gr2 = gr2 + 0x00000001
* 86010003 00000001             # 		Gecko Reg Arith (Immediate): Val @ $(gr3) = Val @ $(gr3) + 0x00000001
* 62000000 00000000             # 	Execute Repeat: Execute Repeat in b0
* E0000000 80008000             # 	Full Terminator: ba = 0x80000000, po = 0x80000000
* 2042BC2C 00000002             # 	32-Bit If Equal: If Val @ $(ba + 0x0042BC2C) == 0x00000002
* 2262B420 00000000             # 	32-Bit If Not Equal: If Val @ $(ba + 0x0062B420) != 0x00000000
* 0442BC2C 00000001             # 	32-Bit Write @ $(ba + 0x0042BC2C):  0x00000001
* E0000000 80008000             # 	Full Terminator: ba = 0x80000000, po = 0x80000000
