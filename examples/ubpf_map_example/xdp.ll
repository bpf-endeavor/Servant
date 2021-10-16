; ModuleID = 'xdp.c'
source_filename = "xdp.c"
target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
target triple = "bpf"

%struct.bpf_map_def = type { i32, i32, i32, i32, i32 }
%struct.xdp_md = type { i32, i32, i32, i32, i32 }

@xsks_map = dso_local global %struct.bpf_map_def { i32 17, i32 4, i32 4, i32 64, i32 0 }, section "maps", align 4, !dbg !0
@test_map = dso_local global %struct.bpf_map_def { i32 2, i32 4, i32 4, i32 1, i32 0 }, section "maps", align 4, !dbg !17
@llvm.used = appending global [3 x i8*] [i8* bitcast (i32 (%struct.xdp_md*)* @_ubpf_map_test to i8*), i8* bitcast (%struct.bpf_map_def* @test_map to i8*), i8* bitcast (%struct.bpf_map_def* @xsks_map to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define dso_local i32 @_ubpf_map_test(%struct.xdp_md* nocapture readonly %0) #0 section "ubpf_map_test" !dbg !49 {
  %2 = alloca i32, align 4
  call void @llvm.dbg.value(metadata %struct.xdp_md* %0, metadata !62, metadata !DIExpression()), !dbg !67
  %3 = bitcast i32* %2 to i8*, !dbg !68
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %3) #3, !dbg !68
  call void @llvm.dbg.value(metadata i32 0, metadata !63, metadata !DIExpression()), !dbg !67
  store i32 0, i32* %2, align 4, !dbg !69, !tbaa !70
  call void @llvm.dbg.value(metadata i32* %2, metadata !63, metadata !DIExpression(DW_OP_deref)), !dbg !67
  %4 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @test_map to i8*), i8* nonnull %3) #3, !dbg !74
  %5 = bitcast i8* %4 to i32*, !dbg !74
  call void @llvm.dbg.value(metadata i32* %5, metadata !65, metadata !DIExpression()), !dbg !67
  %6 = icmp eq i8* %4, null, !dbg !75
  br i1 %6, label %14, label %7, !dbg !77

7:                                                ; preds = %1
  %8 = load i32, i32* %5, align 4, !dbg !78, !tbaa !70
  %9 = add i32 %8, 1, !dbg !79
  store i32 %9, i32* %5, align 4, !dbg !80, !tbaa !70
  %10 = getelementptr inbounds %struct.xdp_md, %struct.xdp_md* %0, i64 0, i32 4, !dbg !81
  %11 = load i32, i32* %10, align 4, !dbg !81, !tbaa !82
  %12 = call i64 inttoptr (i64 51 to i64 (i8*, i32, i64)*)(i8* bitcast (%struct.bpf_map_def* @xsks_map to i8*), i32 %11, i64 2) #3, !dbg !84
  %13 = trunc i64 %12 to i32, !dbg !84
  br label %14, !dbg !85

14:                                               ; preds = %1, %7
  %15 = phi i32 [ %13, %7 ], [ 2, %1 ], !dbg !67
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #3, !dbg !86
  ret i32 %15, !dbg !86
}

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #2

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind willreturn }
attributes #2 = { nounwind readnone speculatable willreturn }
attributes #3 = { nounwind }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!45, !46, !47}
!llvm.ident = !{!48}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "xsks_map", scope: !2, file: !3, line: 12, type: !19, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C99, file: !3, producer: "Ubuntu clang version 11.0.0-2~ubuntu20.04.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, retainedTypes: !14, globals: !16, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "xdp.c", directory: "/home/horntail/servant/examples/ubpf_map_example")
!4 = !{!5}
!5 = !DICompositeType(tag: DW_TAG_enumeration_type, name: "xdp_action", file: !6, line: 3150, baseType: !7, size: 32, elements: !8)
!6 = !DIFile(filename: "/usr/include/linux/bpf.h", directory: "")
!7 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!8 = !{!9, !10, !11, !12, !13}
!9 = !DIEnumerator(name: "XDP_ABORTED", value: 0, isUnsigned: true)
!10 = !DIEnumerator(name: "XDP_DROP", value: 1, isUnsigned: true)
!11 = !DIEnumerator(name: "XDP_PASS", value: 2, isUnsigned: true)
!12 = !DIEnumerator(name: "XDP_TX", value: 3, isUnsigned: true)
!13 = !DIEnumerator(name: "XDP_REDIRECT", value: 4, isUnsigned: true)
!14 = !{!15}
!15 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!16 = !{!0, !17, !27, !35}
!17 = !DIGlobalVariableExpression(var: !18, expr: !DIExpression())
!18 = distinct !DIGlobalVariable(name: "test_map", scope: !2, file: !3, line: 19, type: !19, isLocal: false, isDefinition: true)
!19 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "bpf_map_def", file: !20, line: 130, size: 160, elements: !21)
!20 = !DIFile(filename: "/usr/include/bpf/bpf_helpers.h", directory: "")
!21 = !{!22, !23, !24, !25, !26}
!22 = !DIDerivedType(tag: DW_TAG_member, name: "type", scope: !19, file: !20, line: 131, baseType: !7, size: 32)
!23 = !DIDerivedType(tag: DW_TAG_member, name: "key_size", scope: !19, file: !20, line: 132, baseType: !7, size: 32, offset: 32)
!24 = !DIDerivedType(tag: DW_TAG_member, name: "value_size", scope: !19, file: !20, line: 133, baseType: !7, size: 32, offset: 64)
!25 = !DIDerivedType(tag: DW_TAG_member, name: "max_entries", scope: !19, file: !20, line: 134, baseType: !7, size: 32, offset: 96)
!26 = !DIDerivedType(tag: DW_TAG_member, name: "map_flags", scope: !19, file: !20, line: 135, baseType: !7, size: 32, offset: 128)
!27 = !DIGlobalVariableExpression(var: !28, expr: !DIExpression())
!28 = distinct !DIGlobalVariable(name: "bpf_map_lookup_elem", scope: !2, file: !29, line: 50, type: !30, isLocal: true, isDefinition: true)
!29 = !DIFile(filename: "/usr/include/bpf/bpf_helper_defs.h", directory: "")
!30 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !31, size: 64)
!31 = !DISubroutineType(types: !32)
!32 = !{!15, !15, !33}
!33 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !34, size: 64)
!34 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!35 = !DIGlobalVariableExpression(var: !36, expr: !DIExpression())
!36 = distinct !DIGlobalVariable(name: "bpf_redirect_map", scope: !2, file: !29, line: 1295, type: !37, isLocal: true, isDefinition: true)
!37 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !38, size: 64)
!38 = !DISubroutineType(types: !39)
!39 = !{!40, !15, !41, !43}
!40 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!41 = !DIDerivedType(tag: DW_TAG_typedef, name: "__u32", file: !42, line: 27, baseType: !7)
!42 = !DIFile(filename: "/usr/include/asm-generic/int-ll64.h", directory: "")
!43 = !DIDerivedType(tag: DW_TAG_typedef, name: "__u64", file: !42, line: 31, baseType: !44)
!44 = !DIBasicType(name: "long long unsigned int", size: 64, encoding: DW_ATE_unsigned)
!45 = !{i32 7, !"Dwarf Version", i32 4}
!46 = !{i32 2, !"Debug Info Version", i32 3}
!47 = !{i32 1, !"wchar_size", i32 4}
!48 = !{!"Ubuntu clang version 11.0.0-2~ubuntu20.04.1"}
!49 = distinct !DISubprogram(name: "_ubpf_map_test", scope: !3, file: !3, line: 28, type: !50, scopeLine: 29, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !61)
!50 = !DISubroutineType(types: !51)
!51 = !{!52, !53}
!52 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!53 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !54, size: 64)
!54 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "xdp_md", file: !6, line: 3161, size: 160, elements: !55)
!55 = !{!56, !57, !58, !59, !60}
!56 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !54, file: !6, line: 3162, baseType: !41, size: 32)
!57 = !DIDerivedType(tag: DW_TAG_member, name: "data_end", scope: !54, file: !6, line: 3163, baseType: !41, size: 32, offset: 32)
!58 = !DIDerivedType(tag: DW_TAG_member, name: "data_meta", scope: !54, file: !6, line: 3164, baseType: !41, size: 32, offset: 64)
!59 = !DIDerivedType(tag: DW_TAG_member, name: "ingress_ifindex", scope: !54, file: !6, line: 3166, baseType: !41, size: 32, offset: 96)
!60 = !DIDerivedType(tag: DW_TAG_member, name: "rx_queue_index", scope: !54, file: !6, line: 3167, baseType: !41, size: 32, offset: 128)
!61 = !{!62, !63, !65}
!62 = !DILocalVariable(name: "ctx", arg: 1, scope: !49, file: !3, line: 28, type: !53)
!63 = !DILocalVariable(name: "zero", scope: !49, file: !3, line: 30, type: !64)
!64 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !52)
!65 = !DILocalVariable(name: "val", scope: !49, file: !3, line: 31, type: !66)
!66 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!67 = !DILocation(line: 0, scope: !49)
!68 = !DILocation(line: 30, column: 2, scope: !49)
!69 = !DILocation(line: 30, column: 12, scope: !49)
!70 = !{!71, !71, i64 0}
!71 = !{!"int", !72, i64 0}
!72 = !{!"omnipotent char", !73, i64 0}
!73 = !{!"Simple C/C++ TBAA"}
!74 = !DILocation(line: 32, column: 8, scope: !49)
!75 = !DILocation(line: 33, column: 10, scope: !76)
!76 = distinct !DILexicalBlock(scope: !49, file: !3, line: 33, column: 6)
!77 = !DILocation(line: 33, column: 6, scope: !49)
!78 = !DILocation(line: 35, column: 9, scope: !49)
!79 = !DILocation(line: 35, column: 14, scope: !49)
!80 = !DILocation(line: 35, column: 7, scope: !49)
!81 = !DILocation(line: 36, column: 42, scope: !49)
!82 = !{!83, !71, i64 16}
!83 = !{!"xdp_md", !71, i64 0, !71, i64 4, !71, i64 8, !71, i64 12, !71, i64 16}
!84 = !DILocation(line: 36, column: 9, scope: !49)
!85 = !DILocation(line: 36, column: 2, scope: !49)
!86 = !DILocation(line: 37, column: 1, scope: !49)
