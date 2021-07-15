Change Log
r21.04

Improvements:

FW-3921 Deprecate power management API
FW-3996 Improve interoperability with connection establishment when LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP is enabled
FW-4045 Relocate CIS Audio codec decode to end of CIS ISO event
FW-4047 CIS Should change RTE parameters if it cannot honor host-requested value

Defects fixed:

FW-3851 HCI_LE_CIS_Established event has MAX_PDUs fields zero on both Master and Slave
FW-3879 Incorrect AuxOffsetPtr value with large skip values
FW-3890 Master may not synchronize with PAST
FW-3981 HCI_LE_Setup_ISO_Data_Path for a BIS may incorrectly return success when clearing
FW-3982 HCI_LE_Read_Remote_Features may return bits not in controller-to-controller mask
FW-3986 HCI_Disconnect for CIS should return HCI_ERR_LOCAL_TERMINATED
FW-3987 CIS ISO test Rx mode incorrectly counts successful packets
FW-3989 Framed CIS test Rx mode will hang when receiving a data packet
FW-3995 HCI_LE_Read_Remote_Features with LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP enabled may be disallowed
FW-3997 HCI_LE_Set_Address_Resolution_Enable set to FALSE may prevents controller from generating RPA for peer
FW-3998 HCI_LE_Set_Address_Resolution_Enable set to FALSE may incorrectly continue to resolve RPA packets
FW-3999 HCI_LE_ISO_Receive_Test for BIS does not receive when datapath is disabled
FW-4004 HCI_LE_Transmit_Power_Reporting not generated when delta is 0
FW-4005 Flushing of a PDU when a framed assembly has not started will discard the next valid PDU
FW-4020 Allow payload with SYNC_INFO when synchronizing with Periodic Advertiser
FW-4021 Periodic Synchronizer may use incorrect Aux Offset
FW-4028 IAL BIS case times out before able to complete test procedure
FW-4035 LL initialization directives are multiply defined
FW-4039 LL_FEAT_ISO_SYNC feature bit not set
FW-4042 CIS has MIC failure if two data packets are sent in same ISO event
FW-4044 Peripheral CIS is not able to be schedule after a connection update
FW-4046 CIS acknowledgement scheme does not work correctly
FW-4048 Peripheral CIS may not advance payloads even after central acknowledges them

Known limitations:

FW-3789 ISO timestamps and packet counters may not be aligned in certain scenarios
FW-3886 DLE max packet time should cap at 1M PHY when LE Coded PHY is disabled
FW-4063 HCI_LE_Advertising_Set_Terminated may report incorrect Num_Completed_Extended_Advertising_Events with ExtAdv using LE Coded PHY

r21.02

Improvements:

FW-3988 LeSetCigParams defensive coding for zero-division

Known limitations:

FW-3981 HCI_LE_Setup_ISO_Data_Path for a BIS may incorrectly return success when clearing
FW-3982 HCI_LE_Read_Remote_Features may return bits not in controller-to-controller mask
FW-3986 HCI_Disconnect for CIS should return HCI_ERR_LOCAL_TERMINATED
FW-3789 ISO timestamps and packet counters may not be aligned in certain scenarios
FW-3851 HCI_LE_CIS_Established event has MAX_PDUs fields zero on both Master and Slave
FW-3879 Incorrect AuxOffsetPtr value with large skip values
FW-3886 DLE max packet time should cap at 1M PHY when LE Coded PHY is disabled
FW-3890 Master may not synchronize with PAST
FW-3987 CIS ISO test Rx mode incorrectly counts successful packets
FW-3989 Framed CIS test Rx mode will hang when receiving a data packet
FW-3995 HCI_LE_Read_Remote_Features with LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP enabled may be disallowed
FW-3997 HCI_LE_Set_Address_Resolution_Enable set to FALSE may prevents controller from generating RPA for peer
FW-3998 HCI_LE_Set_Address_Resolution_Enable set to FALSE may incorrectly continue to resolve RPA packets
FW-3999 HCI_LE_ISO_Receive_Test for BIS does not receive when datapath is disabled
FW-4004 HCI_LE_Transmit_Power_Reporting not generated when delta is 0
FW-4005 Flushing of a PDU when a framed assembly has not started will discard the next valid PDU

r20.12

Improvements:

FW-3385 Non-blocking scheme for PAL flash
FW-3728 Packetcraft Audio Cape for PCA10056
FW-3838 Token monitor utility script
FW-3848 CIS and BIS recombination of SDUs
FW-3862 BV32 codec integrated on nRF52840
FW-3864 Write flash between radio events on nRF52840
FW-3865 Nordic secure bootloader updates firmware from internal flash
FW-3876 Tester should have a way to sequentially queue ISO Packets
FW-3899 Controller should queue up disconnect commands for multiple CIS
FW-3905 Change BIS datapath to use generic functions
FW-3907 ITM SWO trace diagnostics
FW-3931 Power control monitor improvements
FW-3943 Sydney erratta: CIS RTE max removed (0x0F -> 0xFF)
FW-3949 LL_OP_MODE_FLAG_FORCE_CIS_CODED_PHY_S2 force CIS to use Coded PHY S2 option

Defects fixed:

FW-3263 Scanner may not filter AdvReport events when Filter Policy is 0x02 and RPA is used
FW-3781 Some broadcaster BIS PTO combinations may fail
FW-3821 BIS broadcaster may corrupt transmitted ISO SDUs
FW-3822 HCI_LE_BIG_Create_Sync does not filter on BIS[i]
FW-3831 HCI_GetVersion does not report build number
FW-3837 Incorrect BIS length will prevent adjacent BIS from receive
FW-3839 CIS interleaved uses incorrect channels on subevent 3+
FW-3841 Timer may expire early
FW-3845 CIS and ACL are using wrong channel parameters
FW-3846 BIS encryption may use incorrect packet counter
FW-3849 Command complete for setting CIG params will always return CIG=0
FW-3850 Disconnecting ACL with CIS does not return a terminated event for CIS
FW-3856 BIS should send lost packet SDUs to the host if no PDU is received
FW-3857 BIS Framed PDUs are not sent in consecutive subevents
FW-3858 Unframed BIS may send duplicate Data PDUs when BN > 1
FW-3868 LE_REMOVE_CIG command doesn't have CIG ID field.
FW-3869 Radio scheduler may operate on list out of order
FW-3870 PHY incorrect when using CIS encryption that differs in PHY with ACL
FW-3872 Removing CIS output data path returns command disallowed
FW-3873 Closing and re-opening the same CIS will not properly establish the second CIS
FW-3875 Controller will not send LLCP anymore if receiving UNKNOWN_RSP for a power control LLCP
FW-3877 LhciRegisterSendTrCompleteHandler() callback does not persist after HCI_Reset
FW-3878 Inline decryption does not filter encrypted packets that are NACK'ed
FW-3881 CIS may attempt transmission after a failed receive
FW-3882 CIS may receive on incorrect PHY when encrypted
FW-3883 CIS rx timeout will cause the whole ISO event to be canceled
FW-3884 CIS Slave encryption will not transmit correctly when bn StoM>0 and NSE>1
FW-3894 CIS Slave will not respond to ACL events if CIS is disconnected
FW-3895 CIS will not pass up lost or invalid SDUs to host
FW-3896 MIC failure upon establishing 2 CIS
FW-3906 Radio scheduler may overlap reservations
FW-3908 Periodic Advertising may not send SyncInfo when restarting
FW-3937 CIS MIC error returns two discconnection events
FW-3941 Advertising while scanning may skip first advertising event
FW-3942 CIS may duplicate packets when using codec datapath

Known limitations:

FW-3789 ISO timestamps and packet counters may not be aligned in certain scenarios
FW-3851 HCI_LE_CIS_Established event has MAX_PDUs fields zero on both Master and Slave
FW-3879 Incorrect AuxOffsetPtr value with large skip values
FW-3886 DLE max packet time should cap at 1M PHY when LE Coded PHY is disabled
FW-3890 Master may not synchronize with PAST
