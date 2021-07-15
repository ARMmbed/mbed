# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

target_include_directories(mbed-ble-cordio
    INTERFACE
        include
        sources/hci/dual_chip
        sources/sec/common
        sources/stack/att
        sources/stack/cfg
        sources/stack/dm
        sources/stack/hci
        sources/stack/l2c
        sources/stack/smp
        ../../TARGET_CORDIO_LL/stack/controller/include/ble
)

target_sources(mbed-ble-cordio
    INTERFACE
        sources/hci/common/hci_core.c

        sources/hci/dual_chip/hci_cmd.c
        sources/hci/dual_chip/hci_cmd_ae.c
        sources/hci/dual_chip/hci_cmd_bis.c
        sources/hci/dual_chip/hci_cmd_cis.c
        sources/hci/dual_chip/hci_cmd_cte.c
        sources/hci/dual_chip/hci_cmd_iso.c
        sources/hci/dual_chip/hci_cmd_past.c
        sources/hci/dual_chip/hci_cmd_phy.c
        sources/hci/dual_chip/hci_core_ps.c
        sources/hci/dual_chip/hci_evt.c
        sources/hci/dual_chip/hci_vs_ae.c

        sources/sec/common/sec_aes.c
        sources/sec/common/sec_aes_rev.c
        sources/sec/common/sec_ccm_hci.c
        sources/sec/common/sec_cmac_hci.c
        sources/sec/common/sec_ecc_debug.c
        sources/sec/common/sec_ecc_hci.c
        sources/sec/common/sec_main.c

        sources/stack/att/att_eatt.c
        sources/stack/att/att_main.c
        sources/stack/att/att_uuid.c
        sources/stack/att/attc_disc.c
        sources/stack/att/attc_eatt.c
        sources/stack/att/attc_main.c
        sources/stack/att/attc_proc.c
        sources/stack/att/attc_read.c
        sources/stack/att/attc_sign.c
        sources/stack/att/attc_write.c
        sources/stack/att/atts_ccc.c
        sources/stack/att/atts_csf.c
        sources/stack/att/atts_dyn.c
        sources/stack/att/atts_eatt.c
        sources/stack/att/atts_ind.c
        sources/stack/att/atts_main.c
        sources/stack/att/atts_proc.c
        sources/stack/att/atts_read.c
        sources/stack/att/atts_sign.c
        sources/stack/att/atts_write.c

        sources/stack/cfg/cfg_stack.c

        sources/stack/dm/dm_adv.c
        sources/stack/dm/dm_adv_ae.c
        sources/stack/dm/dm_adv_leg.c
        sources/stack/dm/dm_bis_master.c
        sources/stack/dm/dm_bis_slave.c
        sources/stack/dm/dm_cis.c
        sources/stack/dm/dm_cis_master.c
        sources/stack/dm/dm_cis_slave.c
        sources/stack/dm/dm_cis_sm.c
        sources/stack/dm/dm_conn.c
        sources/stack/dm/dm_conn_cte.c
        sources/stack/dm/dm_conn_master.c
        sources/stack/dm/dm_conn_master_ae.c
        sources/stack/dm/dm_conn_master_leg.c
        sources/stack/dm/dm_conn_slave.c
        sources/stack/dm/dm_conn_slave_ae.c
        sources/stack/dm/dm_conn_slave_leg.c
        sources/stack/dm/dm_conn_sm.c
        sources/stack/dm/dm_dev.c
        sources/stack/dm/dm_dev_priv.c
        sources/stack/dm/dm_iso.c
        sources/stack/dm/dm_main.c
        sources/stack/dm/dm_past.c
        sources/stack/dm/dm_phy.c
        sources/stack/dm/dm_priv.c
        sources/stack/dm/dm_scan.c
        sources/stack/dm/dm_scan_ae.c
        sources/stack/dm/dm_scan_leg.c
        sources/stack/dm/dm_sec.c
        sources/stack/dm/dm_sec_lesc.c
        sources/stack/dm/dm_sec_master.c
        sources/stack/dm/dm_sec_slave.c
        sources/stack/dm/dm_sync_ae.c

        sources/stack/hci/hci_main.c

        sources/stack/l2c/l2c_coc.c
        sources/stack/l2c/l2c_main.c
        sources/stack/l2c/l2c_master.c
        sources/stack/l2c/l2c_slave.c

        sources/stack/smp/smp_act.c
        sources/stack/smp/smp_db.c
        sources/stack/smp/smp_main.c
        sources/stack/smp/smp_non.c
        sources/stack/smp/smp_sc_act.c
        sources/stack/smp/smp_sc_main.c
        sources/stack/smp/smpi_act.c
        sources/stack/smp/smpi_sc_act.c
        sources/stack/smp/smpi_sc_sm.c
        sources/stack/smp/smpi_sm.c
        sources/stack/smp/smpr_act.c
        sources/stack/smp/smpr_sc_act.c
        sources/stack/smp/smpr_sc_sm.c
        sources/stack/smp/smpr_sm.c
)
