# REQ-0101 — VB6 compiler-helper export ABI

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

WFC shall reproduce the compiler helper ABI used for VB values, arrays, strings, objects, variants, comparisons, arithmetic, conversions, loops, exceptions, errors, file statements, late binding, and procedure cleanup.

The compatibility DLL shall export all 334 symbols below at the
listed ordinals. Reference RVAs identify the inspected binary but are not
required implementation addresses.

| Ordinal | Export | Reference RVA |
| ---: | --- | ---: |
| 108 | `__vbaAryLock` | `0x000EDC0B` |
| 109 | `__vbaBoolErrVar` | `0x000F0EBB` |
| 110 | `__vbaRedimVar2` | `0x000EDA90` |
| 111 | `__vbaStrErrVarCopy` | `0x000F0EAF` |
| 112 | `__vbaVarLateMemCallLd` | `0x0010C281` |
| 113 | `__vbaVarLateMemCallLdRf` | `0x0010CD30` |
| 114 | `__vbaVarLateMemCallSt` | `0x00112D4A` |
| 115 | `__vbaVarLateMemSt` | `0x0010C2BF` |
| 116 | `__vbaVarLateMemStAd` | `0x00112D65` |
| 117 | `__vbaAryVarVarg` | `0x00112006` |
| 118 | `__vbaFpCDblR4` | `0x001120D2` |
| 119 | `__vbaFpCDblR8` | `0x001120D2` |
| 120 | `__vbaFpCSngR4` | `0x001120BB` |
| 121 | `__vbaFpCSngR8` | `0x001120BB` |
| 122 | `__vbaFpCmpCy` | `0x001120E9` |
| 123 | `__vbaFpCy` | `0x0011206A` |
| 124 | `__vbaFpI2` | `0x00106537` |
| 125 | `__vbaFpI4` | `0x00106A18` |
| 126 | `__vbaFpR4` | `0x0010C30E` |
| 127 | `__vbaFpR8` | `0x00106A33` |
| 128 | `__vbaFpUI1` | `0x00112045` |
| 129 | `__vbaFreeObj` | `0x0001D923` |
| 130 | `__vbaFreeStr` | `0x0001D8CD` |
| 131 | `__vbaFreeVar` | `0x001060EF` |
| 132 | `__vbaFreeVarg` | `0x00111CB9` |
| 133 | `__vbaI2Abs` | `0x0005B936` |
| 134 | `__vbaI2I4` | `0x0001D9F5` |
| 135 | `__vbaI2Sgn` | `0x000F3246` |
| 136 | `__vbaI4Abs` | `0x000F321D` |
| 137 | `__vbaI4Sgn` | `0x000F325E` |
| 138 | `__vbaStrCopy` | `0x0001D83C` |
| 139 | `__vbaStrMove` | `0x0001D8B1` |
| 140 | `__vbaUI1I2` | `0x00014F32` |
| 141 | `__vbaUI1I4` | `0x00056811` |
| 142 | `__vbaUI1Sgn` | `0x000F3233` |
| 143 | `__vbaVarCopy` | `0x00106783` |
| 144 | `__vbaVarDup` | `0x00106252` |
| 145 | `__vbaVarMove` | `0x00106987` |
| 146 | `__vbaVarVargNofree` | `0x0010C5D6` |
| 147 | `__vbaVarZero` | `0x00111D9E` |
| 148 | `__vbaVargParmRef` | `0x00111DD3` |
| 149 | `__vbaVargVar` | `0x00111DE0` |
| 150 | `__vbaVargVarCopy` | `0x00111E62` |
| 151 | `__vbaVargVarMove` | `0x00111E52` |
| 152 | `__vbaVargVarRef` | `0x00111E23` |
| 163 | `__vbaAptOffset` | `0x00059B3B` |
| 164 | `__vbaAryConstruct2` | `0x00048817` |
| 165 | `__vbaAryConstruct` | `0x000EDBF1` |
| 166 | `__vbaAryCopy` | `0x000E5C4C` |
| 167 | `__vbaAryDestruct` | `0x00014465` |
| 168 | `__vbaAryMove` | `0x00014600` |
| 169 | `__vbaAryRebase1Var` | `0x000ED88F` |
| 170 | `__vbaAryRecCopy` | `0x000E5C83` |
| 171 | `__vbaAryRecMove` | `0x000E5D00` |
| 172 | `__vbaAryUnlock` | `0x000EDC2E` |
| 173 | `__vbaAryVar` | `0x001114F7` |
| 174 | `__vbaBoolStr` | `0x0003E45F` |
| 175 | `__vbaBoolVar` | `0x0000DD35` |
| 176 | `__vbaBoolVarNull` | `0x001063EB` |
| 177 | `__vbaCVarAryUdt` | `0x000ED8DE` |
| 178 | `__vbaCastObj` | `0x0010622C` |
| 179 | `__vbaCastObjVar` | `0x0001E086` |
| 180 | `__vbaCheckType` | `0x00009DD9` |
| 181 | `__vbaCheckTypeVar` | `0x000EC3B7` |
| 182 | `__vbaChkstk` | `0x0001D7F5` |
| 183 | `__vbaCopyBytes` | `0x000EC834` |
| 184 | `__vbaCopyBytesZero` | `0x000EC859` |
| 185 | `__vbaCyAbs` | `0x000F5DBC` |
| 188 | `__vbaCyAdd` | `0x000F5D0A` |
| 189 | `__vbaCyErrVar` | `0x000F330B` |
| 191 | `__vbaCyFix` | `0x000F5C7B` |
| 192 | `__vbaCyForInit` | `0x00112E32` |
| 193 | `__vbaCyForNext` | `0x00112E5A` |
| 194 | `__vbaCyI2` | `0x000F0E4D` |
| 196 | `__vbaCyI4` | `0x000F0E5F` |
| 197 | `__vbaCyInt` | `0x000F5CAB` |
| 198 | `__vbaCyMul` | `0x000F5D34` |
| 200 | `__vbaCyMulI2` | `0x000F5C26` |
| 201 | `__vbaCySgn` | `0x000F32E1` |
| 202 | `__vbaCyStr` | `0x000F0B3D` |
| 203 | `__vbaCySub` | `0x000F5D1F` |
| 204 | `__vbaCyUI1` | `0x000F0E3B` |
| 205 | `__vbaCyVar` | `0x000F3315` |
| 206 | `__vbaDateR4` | `0x000F0E7D` |
| 207 | `__vbaDateR8` | `0x00014515` |
| 208 | `__vbaDateStr` | `0x0004CC2C` |
| 209 | `__vbaDateVar` | `0x0001DC98` |
| 210 | `__vbaDerefAry` | `0x000ED9D4` |
| 211 | `__vbaDerefAry1` | `0x000ED996` |
| 212 | `__vbaEnd` | `0x000427B8` |
| 213 | `__vbaErase` | `0x00014558` |
| 214 | `__vbaEraseKeepData` | `0x000ED8AF` |
| 215 | `__vbaEraseNoPop` | `0x000ED972` |
| 216 | `__vbaError` | `0x000E6A99` |
| 217 | `__vbaErrorOverflow` | `0x000E6AB2` |
| 218 | `__vbaExceptHandler` | `0x000425BF` |
| 219 | `__vbaExitEachAry` | `0x0011197D` |
| 221 | `__vbaExitEachColl` | `0x0011199F` |
| 222 | `__vbaExitEachVar` | `0x0011195D` |
| 223 | `__vbaExitProc` | `0x0001D962` |
| 224 | `__vbaFPException` | `0x00112024` |
| 225 | `__vbaFPFix` | `0x000485A2` |
| 226 | `__vbaFPInt` | `0x000F5DAA` |
| 228 | `__vbaFailedFriend` | `0x000E592D` |
| 229 | `__vbaFileClose` | `0x0001D937` |
| 233 | `__vbaFileCloseAll` | `0x00056824` |
| 234 | `__vbaFileLock` | `0x000F3C8E` |
| 235 | `__vbaFileOpen` | `0x0000A44A` |
| 236 | `__vbaFileSeek` | `0x000F33B0` |
| 237 | `__vbaFixstrConstruct` | `0x0005682C` |
| 238 | `__vbaForEachAry` | `0x001117ED` |
| 239 | `__vbaForEachCollAd` | `0x0010C6A8` |
| 240 | `__vbaForEachCollObj` | `0x0010C867` |
| 241 | `__vbaForEachCollVar` | `0x0010C6FA` |
| 242 | `__vbaForEachVar` | `0x0011186E` |
| 243 | `__vbaFreeObjList` | `0x0001DA3E` |
| 244 | `__vbaFreeStrList` | `0x0001D8F4` |
| 245 | `__vbaFreeVarList` | `0x001068D4` |
| 246 | `__vbaGenerateBoundsError` | `0x000EDAF4` |
| 247 | `__vbaGet3` | `0x00012523` |
| 248 | `__vbaGet4` | `0x000F34FD` |
| 249 | `__vbaGetFxStr3` | `0x00056863` |
| 250 | `__vbaGetFxStr4` | `0x000F358D` |
| 251 | `__vbaGetOwner3` | `0x000567F5` |
| 252 | `__vbaGetOwner4` | `0x000F3538` |
| 253 | `__vbaGosub` | `0x001115E4` |
| 254 | `__vbaGosubFree` | `0x00111638` |
| 255 | `__vbaGosubReturn` | `0x0011160D` |
| 256 | `__vbaHresultCheck` | `0x000EC8BC` |
| 257 | `__vbaHresultCheckNonvirt` | `0x000EC8CA` |
| 258 | `__vbaHresultCheckObj` | `0x000427C2` |
| 259 | `__vbaI2Cy` | `0x00112097` |
| 260 | `__vbaI2ErrVar` | `0x00055C13` |
| 261 | `__vbaI2ForNextCheck` | `0x00112ED2` |
| 262 | `__vbaI2Str` | `0x00062618` |
| 263 | `__vbaI2Var` | `0x0000E2D8` |
| 264 | `__vbaI4Cy` | `0x001120A9` |
| 267 | `__vbaI4ErrVar` | `0x000410A6` |
| 270 | `__vbaI4ForNextCheck` | `0x00112EFA` |
| 272 | `__vbaI4Str` | `0x0004CB0D` |
| 273 | `__vbaI4Var` | `0x0000DA75` |
| 277 | `__vbaInStr` | `0x00003917` |
| 278 | `__vbaInStrB` | `0x000F434C` |
| 279 | `__vbaInStrVar` | `0x0001227E` |
| 280 | `__vbaInStrVarB` | `0x000F4217` |
| 281 | `__vbaInputFile` | `0x000EACE1` |
| 282 | `__vbaLateIdCall` | `0x0010C2F3` |
| 283 | `__vbaLateIdCallLd` | `0x0010C262` |
| 285 | `__vbaLateIdCallSt` | `0x00112BC7` |
| 286 | `__vbaLateIdNamedCall` | `0x00112C56` |
| 287 | `__vbaLateIdNamedCallLd` | `0x00112C0E` |
| 288 | `__vbaLateIdNamedCallSt` | `0x00112C36` |
| 289 | `__vbaLateIdNamedStAd` | `0x00112C7A` |
| 290 | `__vbaLateIdSt` | `0x0010C249` |
| 291 | `__vbaLateIdStAd` | `0x00112BF5` |
| 292 | `__vbaLateMemCall` | `0x0010693B` |
| 293 | `__vbaLateMemCallLd` | `0x0010695F` |
| 294 | `__vbaLateMemCallSt` | `0x00112C9C` |
| 295 | `__vbaLateMemNamedCall` | `0x00112D04` |
| 296 | `__vbaLateMemNamedCallLd` | `0x00112CBC` |
| 297 | `__vbaLateMemNamedCallSt` | `0x00112CE4` |
| 322 | `__vbaLateMemNamedStAd` | `0x00112D28` |
| 323 | `__vbaLateMemSt` | `0x00106918` |
| 324 | `__vbaLateMemStAd` | `0x0010C29D` |
| 325 | `__vbaLbound` | `0x00044950` |
| 326 | `__vbaLdZeroAry` | `0x0011153F` |
| 327 | `__vbaLenBstr` | `0x0000E251` |
| 328 | `__vbaLenBstrB` | `0x000F3FA5` |
| 329 | `__vbaLenVar` | `0x0004C83A` |
| 330 | `__vbaLenVarB` | `0x000F3ED3` |
| 331 | `__vbaLineInputStr` | `0x000149B8` |
| 332 | `__vbaLineInputVar` | `0x000EA5F6` |
| 333 | `__vbaLsetFixstr` | `0x000568B3` |
| 334 | `__vbaLsetFixstrFree` | `0x000F4454` |
| 335 | `__vbaMidStmtBstr` | `0x000F482C` |
| 336 | `__vbaMidStmtBstrB` | `0x000F48FE` |
| 337 | `__vbaMidStmtVar` | `0x000F4856` |
| 338 | `__vbaMidStmtVarB` | `0x000F487D` |
| 339 | `__vbaNameFile` | `0x000EB78D` |
| 340 | `__vbaNew2` | `0x0001DA6C` |
| 341 | `__vbaNew` | `0x0001DA0B` |
| 342 | `__vbaNextEachAry` | `0x00111672` |
| 343 | `__vbaNextEachCollAd` | `0x0010C916` |
| 344 | `__vbaNextEachCollObj` | `0x0010C8C1` |
| 345 | `__vbaNextEachCollVar` | `0x0010C7D5` |
| 346 | `__vbaNextEachVar` | `0x00111915` |
| 347 | `__vbaObjAddref` | `0x000EC78B` |
| 348 | `__vbaObjIs` | `0x0000B4CE` |
| 349 | `__vbaObjSet` | `0x0001D94B` |
| 350 | `__vbaObjSetAddref` | `0x0001D984` |
| 351 | `__vbaObjVar` | `0x0003E3C9` |
| 352 | `__vbaOnError` | `0x0001D872` |
| 353 | `__vbaOnGoCheck` | `0x000E5D42` |
| 354 | `__vbaPowerR8` | `0x0003F7E1` |
| 355 | `__vbaPrintFile` | `0x0000B6CA` |
| 356 | `__vbaPrintObj` | `0x0005632F` |
| 357 | `__vbaPut3` | `0x00056727` |
| 358 | `__vbaPut4` | `0x000F351B` |
| 359 | `__vbaPutFxStr3` | `0x000F3573` |
| 360 | `__vbaPutFxStr4` | `0x000F35A9` |
| 361 | `__vbaPutOwner3` | `0x00014F45` |
| 362 | `__vbaPutOwner4` | `0x000F3556` |
| 363 | `__vbaR4Cy` | `0x000F0E70` |
| 364 | `__vbaR4ErrVar` | `0x000F331F` |
| 365 | `__vbaR4ForNextCheck` | `0x00112F1E` |
| 366 | `__vbaR4Sgn` | `0x000F3273` |
| 367 | `__vbaR4Str` | `0x0004143F` |
| 368 | `__vbaR4Var` | `0x000448B3` |
| 369 | `__vbaR8Cy` | `0x000486A4` |
| 370 | `__vbaR8ErrVar` | `0x000F3329` |
| 371 | `__vbaR8FixI2` | `0x000F5D65` |
| 372 | `__vbaR8FixI4` | `0x0004858B` |
| 373 | `__vbaR8ForNextCheck` | `0x00106B1E` |
| 374 | `__vbaR8IntI2` | `0x000F5D7C` |
| 375 | `__vbaR8IntI4` | `0x000F5D93` |
| 376 | `__vbaR8Sgn` | `0x000F32AA` |
| 377 | `__vbaR8Str` | `0x000F0B0C` |
| 378 | `__vbaR8Var` | `0x000416F2` |
| 379 | `__vbaRaiseEvent` | `0x00112BDE` |
| 380 | `__vbaRecAnsiToUni` | `0x000ED4A0` |
| 381 | `__vbaRecAssign` | `0x000ED42A` |
| 382 | `__vbaRecDestruct` | `0x000ED40D` |
| 383 | `__vbaRecDestructAnsi` | `0x000ED4DA` |
| 384 | `__vbaRecUniToAnsi` | `0x000ED466` |
| 385 | `__vbaRedim` | `0x000417CC` |
| 386 | `__vbaRedimPreserve` | `0x00014F60` |
| 387 | `__vbaRedimPreserveVar` | `0x000EDA6E` |
| 388 | `__vbaRedimPreserveVar2` | `0x000EDAC2` |
| 389 | `__vbaRedimVar` | `0x000EDA4C` |
| 390 | `__vbaRefVarAry` | `0x00056179` |
| 391 | `__vbaResume` | `0x000F3123` |
| 392 | `__vbaRsetFixstr` | `0x000F455A` |
| 393 | `__vbaRsetFixstrFree` | `0x000F4667` |
| 394 | `__vbaSetSystemError` | `0x0004285F` |
| 395 | `__vbaStopExe` | `0x000E58EF` |
| 396 | `__vbaStr2Vec` | `0x00106B5F` |
| 397 | `__vbaStrAryToAnsi` | `0x000E5B1F` |
| 398 | `__vbaStrAryToUnicode` | `0x000E5B35` |
| 399 | `__vbaStrBool` | `0x000F03EE` |
| 405 | `__vbaStrCat` | `0x0000A323` |
| 406 | `__vbaStrCmp` | `0x0001D8E2` |
| 407 | `__vbaStrComp` | `0x00004FDC` |
| 408 | `__vbaStrCompVar` | `0x000F479F` |
| 409 | `__vbaStrCy` | `0x000F04A5` |
| 415 | `__vbaStrDate` | `0x000F0462` |
| 416 | `__vbaStrFixstr` | `0x000F5C04` |
| 417 | `__vbaStrI2` | `0x0003CE78` |
| 418 | `__vbaStrI4` | `0x00038388` |
| 419 | `__vbaStrLike` | `0x000F5BA6` |
| 422 | `__vbaStrR4` | `0x00041744` |
| 423 | `__vbaStrR8` | `0x00056302` |
| 424 | `__vbaStrTextCmp` | `0x00014014` |
| 425 | `__vbaStrTextLike` | `0x000F5BBB` |
| 426 | `__vbaStrToAnsi` | `0x00031654` |
| 427 | `__vbaStrToUnicode` | `0x000E595C` |
| 428 | `__vbaStrUI1` | `0x000F042F` |
| 429 | `__vbaStrVarCopy` | `0x00057CCC` |
| 437 | `__vbaStrVarMove` | `0x0001D9D4` |
| 438 | `__vbaStrVarVal` | `0x0001E06B` |
| 439 | `__vbaUI1Cy` | `0x00112085` |
| 442 | `__vbaUI1ErrVar` | `0x000F32F7` |
| 443 | `__vbaUI1Str` | `0x000F0E1B` |
| 444 | `__vbaUI1Var` | `0x000F3301` |
| 445 | `__vbaUbound` | `0x000145C1` |
| 446 | `__vbaUdtVar` | `0x001114C8` |
| 447 | `__vbaUnkVar` | `0x00041426` |
| 448 | `__vbaVar2Vec` | `0x0010C62F` |
| 449 | `__vbaVarAbs` | `0x00112140` |
| 450 | `__vbaVarAdd` | `0x00106552` |
| 451 | `__vbaVarAnd` | `0x00106088` |
| 452 | `__vbaVarCat` | `0x00005560` |
| 453 | `__vbaVarCmpEq` | `0x0011307C` |
| 454 | `__vbaVarCmpGe` | `0x001130B1` |
| 455 | `__vbaVarCmpGt` | `0x001130E6` |
| 456 | `__vbaVarCmpLe` | `0x0011311B` |
| 457 | `__vbaVarCmpLt` | `0x0010C217` |
| 458 | `__vbaVarCmpNe` | `0x0010C1AA` |
| 459 | `__vbaVarDateVar` | `0x0001DCBC` |
| 460 | `__vbaVarDiv` | `0x00106AE0` |
| 461 | `__vbaVarEqv` | `0x00112F7B` |
| 462 | `__vbaVarErrI4` | `0x000F0DE3` |
| 463 | `__vbaVarFix` | `0x00112165` |
| 464 | `__vbaVarForInit` | `0x0010C400` |
| 465 | `__vbaVarForNext` | `0x0010C51C` |
| 466 | `__vbaVarIdiv` | `0x0010C5B5` |
| 467 | `__vbaVarImp` | `0x00112FA4` |
| 468 | `__vbaVarIndexLoad` | `0x00056199` |
| 469 | `__vbaVarIndexLoadRef` | `0x0005E9C2` |
| 470 | `__vbaVarIndexLoadRefLock` | `0x000EDAFB` |
| 471 | `__vbaVarIndexStore` | `0x000EDB29` |
| 472 | `__vbaVarIndexStoreObj` | `0x000EDB44` |
| 473 | `__vbaVarInt` | `0x0011218A` |
| 474 | `__vbaVarLike` | `0x000F5B52` |
| 475 | `__vbaVarLikeVar` | `0x000F5BD0` |
| 476 | `__vbaVarMod` | `0x00112117` |
| 477 | `__vbaVarMul` | `0x0010C594` |
| 478 | `__vbaVarNeg` | `0x0010C5F9` |
| 479 | `__vbaVarNot` | `0x00106B01` |
| 480 | `__vbaVarOr` | `0x0010C573` |
| 481 | `__vbaVarPow` | `0x0010CBBC` |
| 482 | `__vbaVarSetObj` | `0x00059C17` |
| 483 | `__vbaVarSetObjAddref` | `0x000EC7A0` |
| 484 | `__vbaVarSetUnk` | `0x000EC7CA` |
| 485 | `__vbaVarSetUnkAddref` | `0x000EC7F4` |
| 486 | `__vbaVarSetVar` | `0x00059C58` |
| 487 | `__vbaVarSetVarAddref` | `0x000EC81E` |
| 488 | `__vbaVarSub` | `0x0010C532` |
| 489 | `__vbaVarTextCmpEq` | `0x00113150` |
| 490 | `__vbaVarTextCmpGe` | `0x001131BA` |
| 491 | `__vbaVarTextCmpGt` | `0x001131EF` |
| 492 | `__vbaVarTextCmpLe` | `0x00113224` |
| 493 | `__vbaVarTextCmpLt` | `0x00113259` |
| 494 | `__vbaVarTextCmpNe` | `0x00113185` |
| 495 | `__vbaVarTextLike` | `0x000F5B7C` |
| 496 | `__vbaVarTextLikeVar` | `0x000F5BEA` |
| 497 | `__vbaVarTextTstEq` | `0x00112FE6` |
| 498 | `__vbaVarTextTstGe` | `0x00113018` |
| 499 | `__vbaVarTextTstGt` | `0x00113031` |
| 500 | `__vbaVarTextTstLe` | `0x0011304A` |
| 501 | `__vbaVarTextTstLt` | `0x00113063` |
| 502 | `__vbaVarTextTstNe` | `0x00112FFF` |
| 503 | `__vbaVarTstEq` | `0x0010C2DA` |
| 504 | `__vbaVarTstGe` | `0x00112FCD` |
| 505 | `__vbaVarTstGt` | `0x0010CB40` |
| 506 | `__vbaVarTstLe` | `0x0010C616` |
| 507 | `__vbaVarTstLt` | `0x00106A47` |
| 508 | `__vbaVarTstNe` | `0x001068FF` |
| 509 | `__vbaVarXor` | `0x00112F52` |
| 510 | `__vbaVargObj` | `0x00111E72` |
| 511 | `__vbaVargObjAddref` | `0x00111EDA` |
| 602 | `__vbaVargUnk` | `0x00111F42` |
| 603 | `__vbaVargUnkAddref` | `0x00111FA4` |
| 604 | `__vbaVerifyVarObj` | `0x000EC893` |
| 623 | `__vbaWriteFile` | `0x00059AF1` |

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
