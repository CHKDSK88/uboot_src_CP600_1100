
With this uboot version you need to make some preparations on the flash.
This is because uboot reads certain board values from flash, and sets them in Read-Only
variables.
The area available on flash is 0xe0000 to 0x100000
Currently uboot uses only the first 1Kb (0x400)

You need to write the correct values to this area.
For this you need to do the following:

1. Clear the area on flash:
> nand erase 0xe0000 0x400

2. Clear memory - we will use it
> mw.b 0x2000000 0 0x400

3. Write magic numbers at the beginnig and end of this block, for uboot to verify its integrity
> mw.l 0x2000000 0xdeadbeef
> mw.l 0x20003f0 0xdeadbeef

4. Write WAN MAC address. In this case it is aa:bb:cc:dd:ee:ff
> mw.l 0x2000010 0xddccbbaa; mw.w 0x2000014 0xffee

5. Write LAN MAC addresses. In this case they are all set to ff:ee:dd:cc:bb:aa
> mw.l 0x2000020 0xccddeeff; mw.w 0x2000024 0xaabb
> mw.l 0x2000030 0xccddeeff; mw.w 0x2000034 0xaabb
> mw.l 0x2000040 0xccddeeff; mw.w 0x2000044 0xaabb
> mw.l 0x2000050 0xccddeeff; mw.w 0x2000054 0xaabb
> mw.l 0x2000060 0xccddeeff; mw.w 0x2000064 0xaabb
> mw.l 0x2000070 0xccddeeff; mw.w 0x2000074 0xaabb
> mw.l 0x2000080 0xccddeeff; mw.w 0x2000084 0xaabb
> mw.l 0x2000090 0xccddeeff; mw.w 0x2000094 0xaabb

6. Write DMZ MAC address. In this case it is set to aa:cc:bb:dd:fe:fe
> mw.l 0x20000a0 0xddbbccaa ; mw.w 0x20000a4 0xfefe

7. Write unitModel - L50'\0' - ascii (Or L30)
                 'L'                    '5'                    '0'                  '\0'
> mw.b 0x20000b0 4c 1; mw.b 0x20000b1 0x35 1; mw.b 0x20000b2 0x30 1; mw.b 0x20000b3 0x0 1

alternatively, write L30
>  mw.b 0x20000b0 4c 1; mw.b 0x20000b1 0x33 1; mw.b 0x20000b2 0x30 1; mw.b 0x20000b3 0x0 1

8. Write unitVer "1" 
                   '1'                  '\0'
> mw.b 0x20000c0 0x31 1; mw.b 0x20000c1 0x0 1

9. Write unitName (you have 64 bytes for this) - in this case it is UTM1
                   'U'                   'T'                    'M'                     '1'                  '\0'
> mw.b 0x20000d0 0x55 1; mw.b 0x20000d1 0x54 1; mw.b 0x20000d2 0x4d 1; mw.b 0x20000d3 0x31 1; mw.b 0x20000d4 0x0 1

10. Now, write from memory to flash:
> nand write.e 0x2000000 0xe0000 0x400 

And... you're done! :)

