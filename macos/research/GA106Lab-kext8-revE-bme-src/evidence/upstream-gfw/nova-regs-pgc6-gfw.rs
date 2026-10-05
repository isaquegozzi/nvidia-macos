// blocks).
//
// These scratch registers remain powered on even in a low-power state and have a designated group
// number.

register! {
    /// Boot Sequence Interface (BSI) register used to determine
    /// if GSP reload/resume has completed during the boot process.
    pub(crate) NV_PGC6_BSI_SECURE_SCRATCH_14(u32) @ 0x001180f8 {
        26:26   boot_stage_3_handoff => bool;
    }

    /// Privilege level mask register. It dictates whether the host CPU has privilege to access the
    /// `PGC6_AON_SECURE_SCRATCH_GROUP_05` register (which it needs to read GFW_BOOT).
    pub(crate) NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK(u32) @ 0x00118128 {
        /// Set after FWSEC lowers its protection level.
        0:0     read_protection_level0 => bool;
    }

    /// OpenRM defines this as a register array, but doesn't specify its size and only uses its
    /// first element. Be conservative until we know the actual size or need to use more registers.
    pub(crate) NV_PGC6_AON_SECURE_SCRATCH_GROUP_05(u32)[1] @ 0x00118234 {}

    /// Scratch group 05 register 0 used as GFW boot progress indicator.
    pub(crate) NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT(u32)
        => NV_PGC6_AON_SECURE_SCRATCH_GROUP_05[0] {
        /// Progress of GFW boot (0xff means completed).
        7:0    progress;
    }

    pub(crate) NV_PGC6_AON_SECURE_SCRATCH_GROUP_42(u32) @ 0x001183a4 {
        31:0    value;
    }

    /// Scratch group 42 register used as framebuffer size.
    pub(crate) NV_USABLE_FB_SIZE_IN_MB(u32) => NV_PGC6_AON_SECURE_SCRATCH_GROUP_42 {
        /// Usable framebuffer size, in megabytes.
        31:0    value;
    }
}

impl NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT {
    /// Returns `true` if GFW boot is completed.
    pub(crate) fn completed(self) -> bool {
        self.progress() == 0xff
    }
}

