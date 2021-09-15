// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Intel Corporation <www.intel.com>
 *
 */

//
// This configuration file has been converted to C using pllconvert.py
// Based on configuration file 2021_0118a Silicom ZL30793.mfg
//


#include "plldata.h"

char PLLFilename[] = "2021_0118a Silicom ZL30793.mfg";

PLLStructItem_t PLLStructItemList[] = {
    // Device Id            : ZL30793
    // GUI Version          : 1.2.2
    // File Generation Date : Monday, January 18, 2021 1:27:48 PM
    // 2009
    //======================================================================

    // NOTE:
    // This is an incremental configuration script.
    // For proper device operation, all register write and wait commands in
    // this file must be performed in the sequence listed.

    //======================================================================

    // Configuration script commands

    // 1.  Register Write Command:
    //        X , <register_address> , <data_bytes>
    //        Both <register_address> and <data_bytes> are in hexadecimal
    //        format and must have the "0x" prefix.
    //        The register_address contains the page number and page offset.
    //        The page number is stored in register_address[14:7].
    //        The page offset is stored in register_address[6:0].

    // 2.  Wait Command:
    //        W , <time_microseconds>
    //        The wait time is specified in microseconds.

    //======================================================================

    // The following lines are used only for the evaluation board GUI configuration:

    // Master Clock Nominal Freq MHz = 125
    // VDDO01 = 2.5 V, VDDO23 = 2.5 V, VDDO45 = 2.5 V, VDDO67 = 2.5 V
    // LoadCap HPOUT0=5.00, HPOUT1=5.00, HPOUT2=5.00, HPOUT3=5.00, HPOUT4=5.00, HPOUT5=5.00, HPOUT6=5.00, HPOUT7=5.00

    //======================================================================

    // Register Configuration Start


    //======================================================================
    // Register Configuration Start

    { 'X', 0x0480, 0x12 }, // hp_ctrl_1
    { 'X', 0x04B0, 0x02 }, // hp_ctrl_2

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x01 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0585, 0x61 }, // ref0p_freq_base
    { 'X', 0x0586, 0xA8 }, // ref0p_freq_base
    { 'X', 0x0587, 0x3D }, // ref0p_freq_mult
    { 'X', 0x0588, 0x09 }, // ref0p_freq_mult
    { 'X', 0x0589, 0x00 }, // ref0p_ratio_m
    { 'X', 0x058A, 0x42 }, // ref0p_ratio_m
    { 'X', 0x058B, 0x00 }, // ref0p_ratio_n
    { 'X', 0x058C, 0x40 }, // ref0p_ratio_n
    { 'X', 0x058D, 0x05 }, // ref0p_config
    { 'X', 0x0598, 0x09 }, // ref0p_pfm_disqualify
    { 'X', 0x0599, 0x60 }, // ref0p_pfm_disqualify
    { 'X', 0x059A, 0x07 }, // ref0p_pfm_qualify
    { 'X', 0x059B, 0x2F }, // ref0p_pfm_qualify
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x02 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x058D, 0x00 }, // ref0n_config
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x04 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0585, 0x61 }, // ref1p_freq_base
    { 'X', 0x0586, 0xA8 }, // ref1p_freq_base
    { 'X', 0x0587, 0x3D }, // ref1p_freq_mult
    { 'X', 0x0588, 0x09 }, // ref1p_freq_mult
    { 'X', 0x0589, 0x8C }, // ref1p_ratio_m
    { 'X', 0x058A, 0x40 }, // ref1p_ratio_m
    { 'X', 0x058B, 0x84 }, // ref1p_ratio_n
    { 'X', 0x058C, 0x00 }, // ref1p_ratio_n
    { 'X', 0x058D, 0x05 }, // ref1p_config
    { 'X', 0x0598, 0x09 }, // ref1p_pfm_disqualify
    { 'X', 0x0599, 0x60 }, // ref1p_pfm_disqualify
    { 'X', 0x059A, 0x07 }, // ref1p_pfm_qualify
    { 'X', 0x059B, 0x2F }, // ref1p_pfm_qualify
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x08 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x058D, 0x00 }, // ref1n_config
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x10 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0585, 0x00 }, // ref2p_freq_base
    { 'X', 0x0586, 0x01 }, // ref2p_freq_base
    { 'X', 0x0587, 0x00 }, // ref2p_freq_mult
    { 'X', 0x0588, 0x01 }, // ref2p_freq_mult
    { 'X', 0x0598, 0x65 }, // ref2p_pfm_disqualify
    { 'X', 0x0599, 0x90 }, // ref2p_pfm_disqualify
    { 'X', 0x059A, 0x4E }, // ref2p_pfm_qualify
    { 'X', 0x059B, 0x20 }, // ref2p_pfm_qualify
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x20 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0587, 0x01 }, // ref2n_freq_mult
    { 'X', 0x0588, 0x90 }, // ref2n_freq_mult
    { 'X', 0x0598, 0x65 }, // ref2n_pfm_disqualify
    { 'X', 0x0599, 0x90 }, // ref2n_pfm_disqualify
    { 'X', 0x059A, 0x4E }, // ref2n_pfm_qualify
    { 'X', 0x059B, 0x20 }, // ref2n_pfm_qualify
    { 'X', 0x05A0, 0x41 }, // ref2n_sync
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x40 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0587, 0x18 }, // ref3p_freq_mult
    { 'X', 0x0588, 0x6A }, // ref3p_freq_mult
    { 'X', 0x0589, 0x00 }, // ref3p_ratio_m
    { 'X', 0x058A, 0x42 }, // ref3p_ratio_m
    { 'X', 0x058B, 0x00 }, // ref3p_ratio_n
    { 'X', 0x058C, 0x40 }, // ref3p_ratio_n
    { 'X', 0x058D, 0x05 }, // ref3p_config
    { 'X', 0x0598, 0x09 }, // ref3p_pfm_disqualify
    { 'X', 0x0599, 0x60 }, // ref3p_pfm_disqualify
    { 'X', 0x059A, 0x07 }, // ref3p_pfm_qualify
    { 'X', 0x059B, 0x2F }, // ref3p_pfm_qualify
    { 'X', 0x05A0, 0x40 }, // ref3p_sync
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x00 }, // ref_mb_mask
    { 'X', 0x0583, 0x80 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x058D, 0x00 }, // ref3n_config
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x01 }, // ref_mb_mask
    { 'X', 0x0583, 0x00 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0585, 0x00 }, // ref4p_freq_base
    { 'X', 0x0586, 0x01 }, // ref4p_freq_base
    { 'X', 0x0587, 0x00 }, // ref4p_freq_mult
    { 'X', 0x0588, 0x01 }, // ref4p_freq_mult
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0582, 0x02 }, // ref_mb_mask
    { 'X', 0x0583, 0x00 }, // ref_mb_mask
    { 'X', 0x0584, 0x02 }, // ref_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0587, 0x03 }, // ref4n_freq_mult
    { 'X', 0x0588, 0x20 }, // ref4n_freq_mult
    { 'X', 0x0584, 0x01 }, // ref_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0602, 0x00 }, // dpll_mb_mask
    { 'X', 0x0603, 0x01 }, // dpll_mb_mask
    { 'X', 0x0604, 0x02 }, // dpll_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0608, 0xEE }, // dpll0_psl
    { 'X', 0x0609, 0x48 }, // dpll0_psl
    { 'X', 0x0610, 0x1E }, // dpll0_ref_sw_mask
    { 'X', 0x0611, 0x1E }, // dpll0_ref_ho_mask
    { 'X', 0x0614, 0xFF }, // dpll0_ref_prio_0
    { 'X', 0x0615, 0xFF }, // dpll0_ref_prio_1
    { 'X', 0x0616, 0x1F }, // dpll0_ref_prio_2
    { 'X', 0x0617, 0xFF }, // dpll0_ref_prio_3
    { 'X', 0x0618, 0xFF }, // dpll0_ref_prio_4
    { 'X', 0x061C, 0x18 }, // dpll0_ho_filter
    { 'X', 0x061D, 0x86 }, // dpll0_ho_delay
    { 'X', 0x061E, 0x01 }, // dpll0_split_xo_config
    { 'X', 0x0630, 0x00 }, // dpll0_phase_bad
    { 'X', 0x0631, 0x1E }, // dpll0_phase_bad
    { 'X', 0x0632, 0x84 }, // dpll0_phase_bad
    { 'X', 0x0633, 0x80 }, // dpll0_phase_bad
    { 'X', 0x0634, 0x00 }, // dpll0_phase_good
    { 'X', 0x0635, 0x0F }, // dpll0_phase_good
    { 'X', 0x0636, 0x42 }, // dpll0_phase_good
    { 'X', 0x0637, 0x40 }, // dpll0_phase_good
    { 'X', 0x063A, 0x01 }, // dpll0_tie
    { 'X', 0x063C, 0x02 }, // dpll0_fp_first_realign
    { 'X', 0x063D, 0x81 }, // dpll0_fp_realign_intvl
    { 'X', 0x0640, 0x00 }, // dpll0_step_time_thresh
    { 'X', 0x0641, 0x00 }, // dpll0_step_time_thresh
    { 'X', 0x0642, 0x4E }, // dpll0_step_time_thresh
    { 'X', 0x0643, 0x20 }, // dpll0_step_time_thresh
    { 'X', 0x0644, 0x00 }, // dpll0_step_time_reso
    { 'X', 0x0645, 0x06 }, // dpll0_step_time_reso
    { 'X', 0x0646, 0x1A }, // dpll0_step_time_reso
    { 'X', 0x0647, 0x80 }, // dpll0_step_time_reso
    { 'X', 0x0604, 0x01 }, // dpll_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0602, 0x00 }, // dpll_mb_mask
    { 'X', 0x0603, 0x02 }, // dpll_mb_mask
    { 'X', 0x0604, 0x02 }, // dpll_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x0605, 0x07 }, // dpll1_bw_fixed
    { 'X', 0x0606, 0x81 }, // dpll1_bw_var
    { 'X', 0x0608, 0x03 }, // dpll1_psl
    { 'X', 0x0609, 0x75 }, // dpll1_psl
    { 'X', 0x0610, 0x1E }, // dpll1_ref_sw_mask
    { 'X', 0x0611, 0x1E }, // dpll1_ref_ho_mask
    { 'X', 0x0614, 0xF0 }, // dpll1_ref_prio_0
    { 'X', 0x0615, 0xF1 }, // dpll1_ref_prio_1
    { 'X', 0x0616, 0xFF }, // dpll1_ref_prio_2
    { 'X', 0x0617, 0xF2 }, // dpll1_ref_prio_3
    { 'X', 0x0618, 0xFF }, // dpll1_ref_prio_4
    { 'X', 0x061C, 0x08 }, // dpll1_ho_filter
    { 'X', 0x061D, 0x86 }, // dpll1_ho_delay
    { 'X', 0x061E, 0x01 }, // dpll1_split_xo_config
    { 'X', 0x0630, 0x00 }, // dpll1_phase_bad
    { 'X', 0x0631, 0x1E }, // dpll1_phase_bad
    { 'X', 0x0632, 0x84 }, // dpll1_phase_bad
    { 'X', 0x0633, 0x80 }, // dpll1_phase_bad
    { 'X', 0x0634, 0x00 }, // dpll1_phase_good
    { 'X', 0x0635, 0x0F }, // dpll1_phase_good
    { 'X', 0x0636, 0x42 }, // dpll1_phase_good
    { 'X', 0x0637, 0x40 }, // dpll1_phase_good
    { 'X', 0x063C, 0x02 }, // dpll1_fp_first_realign
    { 'X', 0x063D, 0x81 }, // dpll1_fp_realign_intvl
    { 'X', 0x0604, 0x01 }, // dpll_mb_sem
    { 'W', 20000, 0 },

    { 'X', 0x0602, 0x00 }, // dpll_mb_mask
    { 'X', 0x0603, 0x08 }, // dpll_mb_mask
    { 'X', 0x0604, 0x02 }, // dpll_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x061E, 0x01 }, // dpll3_split_xo_config
    { 'X', 0x0604, 0x01 }, // dpll_mb_sem
    { 'W', 20000, 0 },
    { 'X', 0x000B, 0x18 }, // central_freq_offset
    { 'X', 0x000C, 0x00 }, // central_freq_offset
    { 'X', 0x000D, 0x72 }, // central_freq_offset
    { 'X', 0x000E, 0xB0 }, // central_freq_offset
    { 'W', 100000, 0 },
    { 'X', 0x020A, 0x99 }, // split_xo_ref
    { 'X', 0x0211, 0x03 }, // dpll_ctrl_0
    { 'X', 0x0238, 0x02 }, // phase_step_mask_gp
    { 'X', 0x023A, 0x03 }, // step_time_mask_gp
    { 'X', 0x023B, 0x20 }, // step_time_mask_hp
    { 'X', 0x0306, 0x18 }, // dpll_df_ctrl_0
    { 'X', 0x0313, 0xFF }, // dpll_tie_data_0
    { 'X', 0x0314, 0xFF }, // dpll_tie_data_0
    { 'X', 0x0315, 0xFF }, // dpll_tie_data_0
    { 'X', 0x0316, 0xFF }, // dpll_tie_data_0
    { 'X', 0x0317, 0x02 }, // dpll_tie_ctrl_0
    { 'X', 0x0326, 0x18 }, // dpll_df_ctrl_1
    { 'X', 0x0337, 0x02 }, // dpll_tie_ctrl_1
    { 'X', 0x0400, 0x01 }, // gp_ctrl
    { 'X', 0x0404, 0x61 }, // gp_freq_base
    { 'X', 0x0405, 0xA8 }, // gp_freq_base
    { 'X', 0x0406, 0x00 }, // gp_freq_mult
    { 'X', 0x0407, 0x72 }, // gp_freq_mult
    { 'X', 0x0408, 0x10 }, // gp_freq_mult
    { 'X', 0x0422, 0x00 }, // gp_out_div_0
    { 'X', 0x0423, 0x00 }, // gp_out_div_0
    { 'X', 0x0424, 0x00 }, // gp_out_div_0
    { 'X', 0x0425, 0x49 }, // gp_out_div_0
    { 'X', 0x0432, 0x2B }, // gp_out_div_1
    { 'X', 0x0433, 0x82 }, // gp_out_div_1
    { 'X', 0x0434, 0xEA }, // gp_out_div_1
    { 'X', 0x0435, 0x80 }, // gp_out_div_1
    { 'X', 0x0450, 0x01 }, // gp_out_en
    { 'X', 0x0450, 0x03 }, // gp_out_en
    { 'X', 0x0491, 0x12 }, // hp_fdiv_base_1
    { 'X', 0x0492, 0x4F }, // hp_fdiv_base_1
    { 'X', 0x0493, 0x80 }, // hp_fdiv_base_1
    { 'X', 0x0494, 0x00 }, // hp_fdiv_base_1
    { 'X', 0x04B4, 0xEE }, // hp_freq_base_2
    { 'X', 0x04B5, 0x6B }, // hp_freq_base_2
    { 'X', 0x04B6, 0x28 }, // hp_freq_base_2
    { 'X', 0x04B7, 0x00 }, // hp_freq_base_2
    { 'X', 0x04C0, 0x08 }, // hp_hsdiv_2
    { 'X', 0x04C1, 0x15 }, // hp_fdiv_base_2
    { 'X', 0x04C2, 0xF9 }, // hp_fdiv_base_2
    { 'X', 0x04C3, 0x00 }, // hp_fdiv_base_2
    { 'X', 0x04C4, 0x00 }, // hp_fdiv_base_2
    { 'X', 0x04E0, 0x7B }, // hp_out_en
    { 'X', 0x04E1, 0xE4 }, // hp_out_mux
    { 'X', 0x0505, 0x02 }, // hp_out_ctrl_0
    { 'X', 0x0515, 0x02 }, // hp_out_ctrl_1
    { 'X', 0x0520, 0x02 }, // hp_out_msdiv_2
    { 'X', 0x0525, 0x02 }, // hp_out_ctrl_2
    { 'X', 0x0530, 0x02 }, // hp_out_msdiv_3
    { 'X', 0x0531, 0x01 }, // hp_out_lsdiv_3
    { 'X', 0x0532, 0xF4 }, // hp_out_lsdiv_3
    { 'X', 0x0533, 0x00 }, // hp_out_lsdiv_3
    { 'X', 0x0534, 0x00 }, // hp_out_lsdiv_3
    { 'X', 0x0535, 0x02 }, // hp_out_ctrl_3
    { 'X', 0x0540, 0x05 }, // hp_out_msdiv_4
    { 'X', 0x0545, 0x02 }, // hp_out_ctrl_4
    { 'X', 0x0550, 0x32 }, // hp_out_msdiv_5
    { 'X', 0x0551, 0x00 }, // hp_out_lsdiv_5
    { 'X', 0x0552, 0x98 }, // hp_out_lsdiv_5
    { 'X', 0x0553, 0x96 }, // hp_out_lsdiv_5
    { 'X', 0x0554, 0x80 }, // hp_out_lsdiv_5
    { 'X', 0x0555, 0x05 }, // hp_out_ctrl_5
    { 'X', 0x0558, 0x10 }, // hp_out_lsctrl_5
    { 'X', 0x0560, 0x02 }, // hp_out_msdiv_6
    { 'X', 0x0565, 0x02 }, // hp_out_ctrl_6
    { 'X', 0x0570, 0x02 }, // hp_out_msdiv_7
    { 'X', 0x0575, 0x02 }, // hp_out_ctrl_7
    { 'X', 0x0480, 0x13 }, // hp_ctrl_1
    { 'X', 0x04B0, 0x03 }, // hp_ctrl_2

    { 'W', 2000000, 0 },
    { 'X', 0x0210, 0x52 }, // dpll_mode_refsel_0
    { 'X', 0x0214, 0x63 }, // dpll_mode_refsel_1
    { 'X', 0x0300, 0x00 }, // dpll_df_offset_0
    { 'X', 0x0301, 0x00 }, // dpll_df_offset_0
    { 'X', 0x0302, 0x00 }, // dpll_df_offset_0
    { 'X', 0x0303, 0x00 }, // dpll_df_offset_0
    { 'X', 0x0304, 0x21 }, // dpll_df_offset_0
    { 'X', 0x0305, 0xAA }, // dpll_df_offset_0
    { 'X', 0x0320, 0xFF }, // dpll_df_offset_1
    { 'X', 0x0321, 0xFF }, // dpll_df_offset_1
    { 'X', 0x0322, 0xFF }, // dpll_df_offset_1
    { 'X', 0x0323, 0xFD }, // dpll_df_offset_1
    { 'X', 0x0324, 0xB7 }, // dpll_df_offset_1
    { 'X', 0x0325, 0xBE }, // dpll_df_offset_1
    { 'X', 0x0209, 0x03 }, // split_xo_ctrl
    { 'W', 2000000, 0 },

    // Register Configuration End
    // Register Write Count = 260

    //======================================================================
};

u32 PLLStructItems = sizeof(PLLStructItemList)/sizeof(PLLStructItem_t);
