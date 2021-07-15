Change Log
r21.04

Improvements:

FW-3921 Deprecate power management API
FW-4022 DM should allow setting of local identity address

Defects fixed:

FW-4018 ATT signed write doesn't take CSRK authentication into account

Known limitations:

FW-3700 Generic Level Set may not use specified steps
FW-3708 Inadequate smooth state transition
FW-3710 Missing bindings
FW-3920 Mesh demo app - problem with Provisioner Server
FW-3929 Some GATT service characteristics security requirements may not match GATT service specs

r21.02

Improvements:

FW-3828 Implement VC and MIC Services and Profiles
FW-3830 Host Isochronous Data Path
FW-3947 ISO Host HCI improvements and support for BIS
FW-4016 DM assert reply on event id instead of event routing table size

Defects fixed:

FW-3928 GATT Profile service characteristics require no authentication
FW-3957 Linux int-to-pointer-cast and pointer-to-int-cast warnings reported with WDXS and EFS
FW-3977 Host terminal button log is wrong
FW-3985 ATT server blocks partial write to any fixed length attribute from offset zero
FW-4014 Missing Medc profile documentation in usage.md
FW-4017 DM Adv APIs use pointer to Adv Data instead of copying actual data

Known limitations:

FW-3434 Mesh Apps should update database hash if they change services
FW-3700 Generic Level Set may not use specified steps
FW-3920 Mesh demo app - problem with Provisioner Server
FW-3929 Some GATT service characteristics security requirements may not match GATT service specs
FW-3950 DM Ext Scan misses final fragments when receiving interleaved AUX ADV segments

r20.12

Improvements:

FW-3385 Non-blocking scheme for PAL flash
FW-3728 Packetcraft Audio Cape for PCA10056
FW-3838 Token monitor utility script
FW-3863 FOTA with Nordic secure bootloader integration
FW-3864 Write flash between radio events on nRF52840
FW-3865 Nordic secure bootloader updates firmware from internal flash
FW-3867 FOTA client profile
FW-3907 ITM SWO trace diagnostics

Defects fixed:

FW-3841 Timer may expire early
FW-3844 ATT silently ignores opcode 0x3F received from PTS
FW-3852 Multiple EATT calls in same task context fail
FW-3855 HCI LE create CIS function doesn't index handles correctly
FW-3892 'Encryption key is ready' flag not reset when DHKeyCheck fails
FW-3933 Allow advertising data to be updated while advertising enabled
FW-3934 attsCsfTable indexed using Connection Handle instead of Connection ID
FW-3944 ConnId used directly as index into ATT table
FW-3945 DM Adv and CTE states set after HCI commands invoked
FW-3946 Assert seen when L2CAP sends ACL data while HCI packets being fragmented
FW-3948 ATTS assert for total number of ATT bearers checks against sizeof(uint8_t)

Known limitations:

FW-3019 Mesh app does not build with USE_EXACTLE=0
FW-3434 Mesh Apps should update database hash if they change services
FW-3700 Generic Level Set may not use specified steps
FW-3920 Mesh demo app - problem with Provisioner Server
FW-3928 GATT Profile service characteristics require no authentication
FW-3950 DM Ext Scan misses final fragments when receiving interleaved AUX ADV segments

