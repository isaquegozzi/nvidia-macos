# M13 part 1 — IOGPU/IOAccelerator interface map (read-only research, no code)

Author: Muse, 2026-09-09 03:43 -0300. Host-only doc. All claims cite exact SDK
symbols read this turn; no Apple header text copied beyond names. Zero production
impact. Directed by valid verdict G0061 §5 (HOLD RELEASED, M13 recommended).

## Sources (macOS SDK 26.5, Xcode 26.5; live kernel 26.6.2)
- `Kernel.framework/Headers/IOKit/graphics/IOAccelerator.h` (43 lines)
- `.../IOKit/graphics/IOAccelTypes.h` (100 lines, `IOACCEL_TYPES_REV 12`)
- `.../IOKit/graphics/IOAccelClientConnect.h` (47 lines)
- `.../IOKit/graphics/IOAccelSurfaceConnect.h` (185 lines)
- `PrivateFrameworks/IOGPU.framework/IOGPU.tbd` (symbols `__globalGPUCommPage`,
  `__ioGPUMemoryInfo`)
- On-disk kexts (presence only): `/System/Library/Extensions/IOGPUFamily.kext`

## Map
1. `IOAccelerator : public IOService`, service name `kIOAcceleratorClassName =
   "IOAccelerator"`. Class surface is minimal: static AccelID lifecycle only —
   `createAccelID / retainAccelID / releaseAccelID(options, id)` with
   `IOAccelID = SInt32` (private IDs flagged `kIOAccelPrivateID = 1`).
2. Client types (`eIOAcceleratorClientTypes`): `kIOAccelSurfaceClientType`
   (count `kIOAccelNumClientTypes`; closed-source `kIOAccelSurface2ClientType =
   0x20`). Future NVIDIA command submission enters through a surface-client
   subclass of this family, NOT through raw selectors on our current service.
3. Surface command boundary (`eIOAccelSurfaceMethods`, the future method table):
   ReadLock/ReadUnlock (+Options variants), WriteLock/WriteUnlock (+Options),
   GetState, Read, SetShape Backing(+AndLength), SetIDMode, SetScale, SetShape,
   Flush, QueryLock, Control. Any future externalMethod dispatch for our GPU
   maps onto a subset of these verbs — lock discipline first, data movement
   second, Flush as the visibility point.
4. Surface descriptors: `IOAccelSurfaceInformation` (4-plane `mach_vm_address_t`
   + rowBytes/width/height/pixelFormat/flags + colorTemperature + typeDependent),
   `IOAccelDeviceRegion` (num_rects + bounds; sized by
   `IOACCEL_SIZEOF_DEVICE_REGION`), `IOAccelSurfaceReadData`,
   `IOAccelSurfaceScaling`. Our future WPR2/FRTS/FB objects surface to clients
   through descriptors of this shape, never raw IOVAs (cf. our N-leak negatives
   and T11–T14 no-address-exposure groups — same principle, new family).
5. Userspace side exists: `IOGPU.framework` (comm page + memory info symbols).
   Kernel side present: `IOGPUFamily.kext`. Integration means bridging our
   GSP-driven engine to THESE interfaces, not inventing new ones.

## Transfer from our current architecture (grounded, own code)
- Our `externalMethod` dispatch-table pattern (selector → fixed struct sizes →
  BadArgument matrix N1–N6) transfers directly to a surface-client method table:
  same zero-input/exact-size discipline, new verbs.
- Our zero-HW proof discipline (mock ops + balanced teardown asserts) is the
  template for pre-live surface-client tests.
- Our fail-closed sequencing (M8 terminal states, no auto-retry) is the template
  for lock/flush error paths.

## NOT-MODELED / explicitly future (needs live GPU + authorization)
Subclassing IOAccelerator, MTLDevice bring-up, command-buffer encoding, fences/
sync, display pipe (IOFramebuffer), any real surface allocation or submission.
Next research step (part 2): `IOAccelSurfaceConnect` method semantics in order
(lock → shape → flush → control) mapped to our engine states; still no code.

## Verdict of this survey
M13 part 1 complete as research: the family vocabulary is now mapped with exact
citations. No interfaces invented. No code written. IOGPU-SURVEY.md deferral is
PARTIALLY lifted for research only (headers found in-SDK); code/execution
deferral stands fully.
