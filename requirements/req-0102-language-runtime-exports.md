# REQ-0102 — VB6 language-runtime service exports

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

WFC shall reproduce the callable language services used for strings, characters, dates and times, numeric and financial functions, formatting, conversion, filesystem and settings access, interaction, locale, random numbers, and array-producing functions.

The compatibility DLL shall export all 198 symbols below at the
listed ordinals. Reference RVAs identify the inspected binary but are not
required implementation addresses.

| Ordinal | Export | Reference RVA |
| ---: | --- | ---: |
| 512 | `rtcLeftBstr` | `0x0001D40F` |
| 513 | `rtcLeftVar` | `0x000388BB` |
| 514 | `rtcRightBstr` | `0x0000D76E` |
| 515 | `rtcRightVar` | `0x000F3FB3` |
| 516 | `rtcAnsiValueBstr` | `0x0001D76D` |
| 517 | `rtcLowerCaseBstr` | `0x0004168E` |
| 518 | `rtcLowerCaseVar` | `0x0004163A` |
| 519 | `rtcTrimBstr` | `0x0000D9F1` |
| 520 | `rtcTrimVar` | `0x000F43F3` |
| 521 | `rtcLeftTrimBstr` | `0x000F4495` |
| 522 | `rtcLeftTrimVar` | `0x000F44B5` |
| 523 | `rtcRightTrimBstr` | `0x000F46A8` |
| 524 | `rtcRightTrimVar` | `0x000F46C8` |
| 525 | `rtcSpaceBstr` | `0x000F49C3` |
| 526 | `rtcSpaceVar` | `0x000F4A05` |
| 527 | `rtcUpperCaseBstr` | `0x00040F46` |
| 528 | `rtcUpperCaseVar` | `0x00040EF2` |
| 529 | `rtcKillFiles` | `0x0001D46C` |
| 530 | `rtcChangeDir` | `0x000EB535` |
| 531 | `rtcMakeDir` | `0x000676BA` |
| 532 | `rtcRemoveDir` | `0x000EB556` |
| 533 | `rtcChangeDrive` | `0x000EB806` |
| 534 | `rtcBeep` | `0x000EDC47` |
| 535 | `rtcGetTimer` | `0x00005511` |
| 536 | `rtcStrFromVar` | `0x000480C3` |
| 537 | `rtcBstrFromAnsi` | `0x0001DF85` |
| 538 | `rtcPackDate` | `0x000E9205` |
| 539 | `rtcPackTime` | `0x000E9265` |
| 540 | `rtcGetDateValue` | `0x000E92CB` |
| 541 | `rtcGetTimeValue` | `0x000E933A` |
| 542 | `rtcGetDayOfMonth` | `0x0004CCF3` |
| 543 | `rtcGetHourOfDay` | `0x000E9414` |
| 544 | `rtcGetMinuteOfHour` | `0x000E9457` |
| 545 | `rtcGetMonthOfYear` | `0x0004CC67` |
| 546 | `rtcGetPresentDate` | `0x0001DE9B` |
| 547 | `rtcGetSecondOfMinute` | `0x000E949A` |
| 548 | `rtcSetDateVar` | `0x000E9539` |
| 549 | `rtcSetDateBstr` | `0x000E9583` |
| 550 | `rtcSetTimeVar` | `0x000E976F` |
| 551 | `rtcSetTimeBstr` | `0x000E97C6` |
| 552 | `rtcGetDayOfWeek` | `0x000E93A4` |
| 553 | `rtcGetYear` | `0x0004CD32` |
| 554 | `rtcFileReset` | `0x000EAD99` |
| 555 | `rtcFileAttributes` | `0x000EAE2A` |
| 556 | `rtcIsArray` | `0x000EDC63` |
| 557 | `rtcIsDate` | `0x000EDC77` |
| 558 | `rtcIsEmpty` | `0x000EDD4E` |
| 559 | `rtcIsError` | `0x000EDD62` |
| 560 | `rtcIsNull` | `0x00049710` |
| 561 | `rtcIsNumeric` | `0x000EDD7A` |
| 562 | `rtcIsObject` | `0x00059C41` |
| 563 | `rtcVarType` | `0x0003E930` |
| 564 | `rtDecFromVar` | `0x000F0155` |
| 565 | `rtcFileWidth` | `0x000EADC0` |
| 566 | `rtcInputCount` | `0x000EA678` |
| 567 | `rtcInputCountVar` | `0x000EA64A` |
| 568 | `rtcFileSeek` | `0x000F3333` |
| 569 | `rtcFileLocation` | `0x000F346F` |
| 570 | `rtcFileLength` | `0x00012476` |
| 571 | `rtcEndOfFile` | `0x00011178` |
| 572 | `rtcHexBstrFromVar` | `0x00040FCA` |
| 573 | `rtcHexVarFromVar` | `0x00041070` |
| 574 | `rtcOctBstrFromVar` | `0x000F0B7E` |
| 575 | `rtcOctVarFromVar` | `0x000F0C66` |
| 576 | `rtcFileCopy` | `0x00014634` |
| 577 | `rtcFileDateTime` | `0x000EB567` |
| 578 | `rtcFileLen` | `0x0005668D` |
| 579 | `rtcGetFileAttr` | `0x000EB632` |
| 580 | `rtcSetFileAttr` | `0x000EB65B` |
| 581 | `rtcR8ValFromBstr` | `0x0001253F` |
| 582 | `rtcSin` | `0x000EDE9E` |
| 583 | `rtcCos` | `0x000EDEC7` |
| 584 | `rtcTan` | `0x000EDEF0` |
| 585 | `rtcAtn` | `0x000EDF20` |
| 586 | `rtcExp` | `0x0005E9EC` |
| 587 | `rtcLog` | `0x0005615B` |
| 588 | `rtcRgb` | `0x00041770` |
| 589 | `rtcQBColor` | `0x000410F8` |
| 590 | `rtcMacId` | `0x000EAF01` |
| 591 | `rtcTypeName` | `0x000439CE` |
| 592 | `rtcIsMissing` | `0x0000E74F` |
| 593 | `rtcRandomNext` | `0x0004871A` |
| 594 | `rtcRandomize` | `0x000486B1` |
| 595 | `rtcMsgBox` | `0x000EE26A` |
| 596 | `rtcInputBox` | `0x000EE439` |
| 597 | `rtcAppActivate` | `0x000EDF7A` |
| 598 | `rtcDoEvents` | `0x00009978` |
| 599 | `rtcSendKeys` | `0x00048894` |
| 600 | `rtcShell` | `0x000E6633` |
| 601 | `rtcArray` | `0x0010CC81` |
| 605 | `rtcGetErl` | `0x000EDC50` |
| 606 | `rtcStringBstr` | `0x0004059D` |
| 607 | `rtcStringVar` | `0x00040564` |
| 608 | `rtcVarBstrFromAnsi` | `0x00041B7C` |
| 609 | `rtcGetDateBstr` | `0x000E9A8F` |
| 610 | `rtcGetDateVar` | `0x0004CB35` |
| 611 | `rtcGetTimeBstr` | `0x000E9B51` |
| 612 | `rtcGetTimeVar` | `0x000E9A41` |
| 613 | `rtcVarStrFromVar` | `0x00048366` |
| 614 | `rtcSqr` | `0x000EDF2B` |
| 615 | `rtcIMEStatus` | `0x000EE81C` |
| 616 | `rtcLeftCharBstr` | `0x0001D459` |
| 617 | `rtcLeftCharVar` | `0x00038890` |
| 618 | `rtcRightCharBstr` | `0x0000D75B` |
| 619 | `rtcRightCharVar` | `0x000F4017` |
| 620 | `rtcInputCharCount` | `0x000EA76B` |
| 621 | `rtcInputCharCountVar` | `0x000EA73D` |
| 622 | `rtcStrConvVar` | `0x000F5605` |
| 624 | `rtcGetHostLCID` | `0x000E659A` |
| 625 | `rtcCreateObject` | `0x000EC429` |
| 626 | `rtcGetObject` | `0x000EC4BB` |
| 627 | `rtcAppleScript` | `0x000FEEE6` |
| 628 | `rtcMidBstr` | `0x0000E1B1` |
| 629 | `rtcMidVar` | `0x000F4042` |
| 630 | `rtcInStr` | `0x000F4183` |
| 631 | `rtcMidCharBstr` | `0x0000E162` |
| 632 | `rtcMidCharVar` | `0x00040E98` |
| 633 | `rtcInStrChar` | `0x000F40EE` |
| 634 | `rtBstrFromErrVar` | `0x000F04D8` |
| 635 | `rtBoolFromErrVar` | `0x000F0961` |
| 636 | `rtCyFromErrVar` | `0x000F0381` |
| 637 | `rtI2FromErrVar` | `0x000F01AD` |
| 638 | `rtI4FromErrVar` | `0x000F021C` |
| 639 | `rtR4FromErrVar` | `0x000F027B` |
| 640 | `rtR8FromErrVar` | `0x000F02E8` |
| 641 | `rtcDateFromVar` | `0x000F092B` |
| 642 | `rtcVarFromVar` | `0x000F060D` |
| 643 | `rtcCVErrFromVar` | `0x000F0649` |
| 645 | `rtcDir` | `0x000101CE` |
| 646 | `rtcCurrentDirBstr` | `0x000EB388` |
| 647 | `rtcCurrentDir` | `0x000EB2DF` |
| 648 | `rtcFreeFile` | `0x0000B540` |
| 649 | `rtcCompareBstr` | `0x000F4773` |
| 650 | `rtcBstrFromFormatVar` | `0x0001DDE0` |
| 651 | `rtcBstrFromError` | `0x0003E839` |
| 652 | `rtcVarFromError` | `0x0003E80E` |
| 653 | `rtcLenCharVar` | `0x000F3F59` |
| 654 | `rtcLenVar` | `0x000F3F7F` |
| 655 | `rtcFixVar` | `0x000F068B` |
| 656 | `rtcAbsVar` | `0x000F06DB` |
| 657 | `rtcIntVar` | `0x000F072B` |
| 658 | `rtcSgnVar` | `0x000F077B` |
| 660 | `rtcVarFromFormatVar` | `0x000FEEED` |
| 661 | `rtcDateAdd` | `0x000E9CE6` |
| 662 | `rtcDateDiff` | `0x000E9F21` |
| 663 | `rtcDatePart` | `0x000EA255` |
| 664 | `rtcPartition` | `0x000EE835` |
| 665 | `rtcChoose` | `0x000EEAF9` |
| 666 | `rtcEnvironVar` | `0x00056662` |
| 667 | `rtcEnvironBstr` | `0x000115B8` |
| 668 | `rtcSwitch` | `0x000EEBBB` |
| 669 | `rtcCommandBstr` | `0x00031480` |
| 670 | `rtcCommandVar` | `0x0003BBC3` |
| 671 | `rtcSLN` | `0x000FF0C1` |
| 672 | `rtcSYD` | `0x000FF0E6` |
| 673 | `rtcDDB` | `0x000FF13C` |
| 674 | `rtcIPMT` | `0x000FF27C` |
| 675 | `rtcPPMT` | `0x000FF36B` |
| 676 | `rtcPMT` | `0x000FF41F` |
| 677 | `rtcPV` | `0x000FF4E7` |
| 678 | `rtcFV` | `0x000FF5A2` |
| 679 | `rtcNPer` | `0x000FF66D` |
| 680 | `rtcRate` | `0x000FF791` |
| 681 | `rtcImmediateIf` | `0x0004296D` |
| 682 | `rtcIRR` | `0x000FF998` |
| 683 | `rtcMIRR` | `0x000FFC89` |
| 684 | `rtcNPV` | `0x000FFE23` |
| 685 | `rtcErrObj` | `0x0000B728` |
| 686 | `rtUI1FromErrVar` | `0x000F09B4` |
| 687 | `rtcVarDateFromVar` | `0x000F0937` |
| 689 | `rtcGetSetting` | `0x000FFFFE` |
| 690 | `rtcSaveSetting` | `0x001001B4` |
| 691 | `rtcDeleteSetting` | `0x0010026D` |
| 692 | `rtcGetAllSettings` | `0x001005F9` |
| 693 | `rtcByteValueBstr` | `0x000F40A9` |
| 694 | `rtcBstrFromByte` | `0x000F0A28` |
| 695 | `rtcVarBstrFromByte` | `0x000F0A49` |
| 696 | `rtcCharValueBstr` | `0x000F40CA` |
| 697 | `rtcBstrFromChar` | `0x000F0A74` |
| 698 | `rtcVarBstrFromChar` | `0x000F0AB5` |
| 699 | `rtcSetCurrentCalendar` | `0x000EA5CF` |
| 700 | `rtcGetCurrentCalendar` | `0x00004FCC` |
| 702 | `rtcFormatNumber` | `0x000FEF64` |
| 703 | `rtcFormatCurrency` | `0x000FEFFC` |
| 704 | `rtcFormatPercent` | `0x000FEFB0` |
| 705 | `rtcFormatDateTime` | `0x000FEF21` |
| 706 | `rtcWeekdayName` | `0x000FF048` |
| 707 | `rtcMonthName` | `0x000FF086` |
| 708 | `rtcFilter` | `0x00100F64` |
| 709 | `rtcInStrRev` | `0x00100D5B` |
| 710 | `rtcJoin` | `0x001011D8` |
| 711 | `rtcSplit` | `0x0010089D` |
| 712 | `rtcReplace` | `0x00101486` |
| 713 | `rtcStrReverse` | `0x00101433` |
| 714 | `rtcRound` | `0x00012439` |
| 715 | `rtcCallByName` | `0x000EEC33` |
| 716 | `rtcCreateObject2` | `0x00058F01` |
| 717 | `rtcStrConvVar2` | `0x000F562F` |

## Rationale

Native-code VB6 programs import runtime entry points directly. Their
names, ordinals, calling conventions, data layouts, and behavior therefore
form a binary compatibility boundary independent of the VBA object model.

## Verification

**Method:** Inspection and test

**References:** `evidence/reference/msvbvm60-6.0-runtime.json`; test case TBD

Link representative VB6 native executables that import every listed helper family, compare import resolution by name and ordinal, and compare observable results, errors, state changes, and cleanup against the reference runtime.

## Relationships

- **Derived from:** MP-0001 compatibility discovery objective
- **Depends on:** WCRT integration and VB execution-engine architecture
- **Conflicts with:** None known

## Tailoring

An export or behavior may be deferred only through an approved compatibility
profile that identifies the affected compiled VB6 programs.

## Implementation Record

Not implemented.
