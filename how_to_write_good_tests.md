# How to Write Good Tests for OpenSTA

OpenSTA test review (2025-2026)에서 발견된 주요 품질 문제를 정리한 가이드.
향후 테스트 작성 및 리뷰 시 체크리스트로 활용할 것.

---

## 1. Inline Data File Creation 금지

테스트 내에서 임시 파일을 생성하여 데이터를 만들지 말 것.
체크인된 테스트 데이터 파일(`test/nangate45/`, `test/sky130hd/` 등)을 사용해야 한다.

**Bad:**
```cpp
// 임시 .lib 파일을 C++ 문자열로 직접 생성
static const char *lib_content = R"(
library(test_lib) {
  ...
}
)";
FILE *f = fopen("/tmp/test.lib", "w");
fprintf(f, "%s", lib_content);
fclose(f);
sta->readLiberty("/tmp/test.lib", ...);
```

**Good:**
```cpp
// 체크인된 실제 liberty 파일 사용
LibertyLibrary *lib = sta->readLiberty(
    "test/nangate45/Nangate45_typ.lib",
    sta->cmdCorner(), MinMaxAll::min(), false);
ASSERT_NE(lib, nullptr);
```

**이유:**
- 임시 파일 정리 실패 시 테스트 환경 오염
- 코드 리뷰 시 데이터와 로직이 혼재되어 가독성 저하

---

## 2. catch 사용 금지

Tcl 테스트에서 `catch` 블록으로 에러를 삼키지 말 것.
에러가 발생하면 테스트가 실패해야 한다.

**Bad:**
```tcl
# 에러를 삼켜서 테스트가 항상 성공하는 것처럼 보임
catch { report_checks -from [get_ports in1] } result
puts "PASS: report_checks"
```

**Good:**
```tcl
# 에러 발생 시 테스트 자체가 실패함
report_checks -from [get_ports in1] -to [get_ports out1]
```

**catch가 허용되는 유일한 경우:** 에러 발생 자체를 검증하는 테스트
```tcl
# 존재하지 않는 셀을 찾을 때 에러가 발생하는지 확인
if { [catch { get_lib_cells NONEXISTENT } msg] } {
  puts "Expected error: $msg"
} else {
  puts "FAIL: should have raised error"
}
```

C++ 테스트에서도 동일한 원칙 적용:
```cpp
// Bad: 예외를 삼키는 것
ASSERT_NO_THROW(some_function());  // 이것만으로 의미 없음

// Good: 실제 결과를 검증
some_function();
EXPECT_EQ(result, expected_value);

// 예외 발생을 테스트하는 경우만 EXPECT_THROW 사용
EXPECT_THROW(sta->endpoints(), Exception);
```

---

## 3. 테스트 당 최소 5개 이상의 유의미한 assertion

단순 존재 확인(null 체크, 파일 존재 여부)만으로는 충분하지 않다.
각 테스트 케이스는 기능의 정확성을 검증하는 최소 5개 이상의 checker를 포함해야 한다.

**Bad:**
```cpp
// 존재 확인만 하는 무의미한 테스트
TEST_F(StaDesignTest, BasicCheck) {
  EXPECT_NE(sta_, nullptr);           // 1: null 아님 -- 당연함
  EXPECT_NE(sta_->network(), nullptr); // 2: null 아님 -- 당연함
  SUCCEED();                          // 3: 아무것도 검증 안 함
}
```

```tcl
# 파일 존재 여부만 확인하는 무의미한 Tcl 테스트
write_verilog $out1
if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: output file exists"
}
```

**Good:**
```cpp
TEST_F(StaDesignTest, TimingAnalysis) {
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(MinMax::max(), worst_slack, worst_vertex);

  EXPECT_NE(worst_vertex, nullptr);                    // 1: vertex 존재
  EXPECT_LT(worst_slack, 0.0f);                        // 2: slack 범위 확인
  EXPECT_GT(worst_slack, -1e6);                         // 3: 합리적 범위
  EXPECT_NE(sta_->graph()->pinDrvrVertex(             // 4: graph 연결성
      sta_->network()->findPin("r1/D")), nullptr);
  PathEndSeq ends = sta_->findPathEnds(...);
  EXPECT_GT(ends.size(), 0u);                          // 5: path 존재
}
```

```tcl
# 실제 타이밍 값을 검증하는 Tcl 테스트
read_liberty test/nangate45/Nangate45_typ.lib
read_verilog examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
report_checks -from [get_ports in1] -to [get_ports out1]
# .ok 파일의 diff로 slack, delay, path를 모두 검증
```

**유의미한 assertion 예시:**
- 타이밍 값(slack, delay, slew) 범위 검증
- 셀/포트/핀의 속성값 비교
- 그래프 연결성 확인
- 리포트 출력의 golden file diff
- 에러 조건에서의 예외 발생 확인

---

## 4. 너무 큰 .cc 파일 지양

단일 테스트 소스 파일은 **5,000줄 이하**를 유지할 것.
GCC는 대형 파일에서 `variable tracking size limit exceeded` 경고를 발생시키며,
이는 디버그 빌드의 품질을 저하시킨다.

**실제 사례:**
- `TestLiberty.cc` (14,612줄) -> 3개 파일로 분할
- `TestSearch.cc` (20,233줄) -> 4개 파일로 분할

**분할 기준:**
- 테스트 fixture 클래스 별로 분할 (서로 다른 SetUp/TearDown)
- 기능 영역별 분할 (class tests / Sta-based tests / callback tests)
- 의존성 수준별 분할 (standalone / Tcl required / design loaded)

**CMakeLists.txt에서 매크로로 반복 제거:**
```cmake
macro(sta_cpp_test name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} OpenSTA GTest::gtest GTest::gtest_main ${TCL_LIBRARY})
  target_include_directories(${name} PRIVATE ${STA_HOME}/include/sta ${STA_HOME} ${CMAKE_BINARY_DIR}/include/sta)
  gtest_discover_tests(${name} WORKING_DIRECTORY ${STA_HOME} PROPERTIES LABELS "cpp;module_liberty")
endmacro()

sta_cpp_test(TestLibertyClasses)
sta_cpp_test(TestLibertyStaBasics)
sta_cpp_test(TestLibertyStaCallbacks)
```

**부수 효과:** 파일 분할은 빌드 병렬성도 향상시킨다.

---

## 5. `puts "PASS: ..."` 출력 금지 (Tcl 테스트)

Tcl 테스트의 합격/불합격은 `.ok` golden file과의 diff로 결정된다.
`puts "PASS: ..."` 는 테스트가 실패해도 항상 출력되므로 혼란만 초래한다.

**Bad:**
```tcl
report_checks
puts "PASS: baseline timing"

set_wire_load_model "large"
report_checks
puts "PASS: large wireload"
```

**Good:**
```tcl
report_checks

set_wire_load_model "large"
report_checks
# .ok 파일 diff가 검증을 수행함
```

---

## 6. Tcl 테스트는 독립적으로 실행 가능해야 함

하나의 `.tcl` 파일에 여러 독립 테스트를 넣지 말 것.
디버깅 시 특정 테스트만 실행할 수 없게 된다.

**Bad:**
```tcl
# 하나의 파일에 10개 테스트가 모두 들어있음
# 3번째 테스트를 디버깅하려면 1,2번을 먼저 실행해야 함
read_liberty lib1.lib
puts "PASS: read lib1"
read_liberty lib2.lib
puts "PASS: read lib2"
# ... 반복
```

**Good:**
```tcl
# liberty_read_nangate.tcl - Nangate45 라이브러리 읽기 및 검증
read_liberty ../../test/nangate45/Nangate45_typ.lib
report_lib_cell NangateOpenCellLibrary/BUF_X1
report_lib_cell NangateOpenCellLibrary/INV_X1
# 하나의 주제에 집중하는 독립 테스트
```

동일 liberty를 여러 번 로드하면 `Warning: library already exists` 경고가 발생하므로,
각 테스트 파일은 자체 환경을 구성해야 한다.

---

## 7. Load-only 테스트 금지

파일을 읽기만 하고 내용을 검증하지 않는 테스트는 무의미하다.

**Bad:**
```tcl
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF SLVT TT"
# 라이브러리를 읽기만 하고, 셀/포트/타이밍 아크를 전혀 검증하지 않음
```

**Good:**
```tcl
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz
report_lib_cell asap7sc7p5t/INVx1_ASAP7_75t_SL
# .ok diff가 셀 정보, 타이밍 모델, 핀 방향 등을 검증
```

---

## 8. Stale 주석 금지

커버리지 라인 번호, hit count 등 시간이 지나면 달라지는 정보를 주석에 넣지 말 것.

**Bad:**
```tcl
# Targets: VerilogWriter.cc uncovered functions:
#   writeInstBusPin (line 382, hit=0), writeInstBusPinBit (line 416, hit=0)
```

**Good:**
```tcl
# Test write_verilog with bus pins and bit-level connections
```

---

## 9. `EXPECT_TRUE(true)` / `SUCCEED()` 금지 (C++ 테스트)

아무것도 검증하지 않는 assertion은 테스트가 아니다.

**Bad:**
```cpp
TEST_F(SdcTest, ClkNameLess) {
  ClkNameLess cmp;
  (void)cmp;
  EXPECT_TRUE(true);  // "컴파일 되면 성공" -- 의미 없음
}
```

**Good:**
```cpp
TEST_F(SdcTest, ClkNameLess) {
  Clock *clk_a = makeClock("aaa", ...);
  Clock *clk_b = makeClock("bbb", ...);
  ClkNameLess cmp;
  EXPECT_TRUE(cmp(clk_a, clk_b));   // "aaa" < "bbb"
  EXPECT_FALSE(cmp(clk_b, clk_a));  // "bbb" < "aaa" is false
  EXPECT_FALSE(cmp(clk_a, clk_a));  // reflexive
}
```

---

## 10. 코어 기능은 C++ 테스트로

Tcl 테스트는 SDC 명령어, 리포트 형식 등 사용자 인터페이스 검증에 적합하다.
delay calculation, graph traversal 등 코어 엔진 로직은 C++ 테스트로 작성해야 한다.

| 적합 | C++ 테스트 | Tcl 테스트 |
|------|-----------|-----------|
| 예시 | delay calc, graph ops, path search | SDC commands, report formats, write_verilog |
| 장점 | 빠름, 디버거 연동, 세밀한 검증 | golden file diff, 실제 사용 시나리오 |
| 단점 | 셋업 코드 필요 | 느림, 디버깅 어려움 |

---

## 11. Golden File(.ok) 관리

- 모든 Tcl 테스트는 대응하는 `.ok` 파일이 있어야 함 (orphan 방지)
- `.ok` 파일 없는 Tcl 테스트는 dead test -- 삭제하거나 `.ok` 생성
- `.ok` 파일만 있고 `.tcl`이 없는 경우도 정리 대상

---

## 체크리스트 요약

테스트 PR 리뷰 시 다음을 확인:

- [ ] 인라인 데이터 파일 생성 없음 (체크인된 테스트 데이터 사용)
- [ ] `catch` 블록 없음 (에러 테스트 목적 제외)
- [ ] 테스트 당 assertion 5개 이상 (null 체크만으로 불충분)
- [ ] 소스 파일 5,000줄 이하
- [ ] `puts "PASS: ..."` 없음
- [ ] `EXPECT_TRUE(true)` / `SUCCEED()` 없음
- [ ] load-only 테스트 없음 (읽은 데이터를 반드시 검증)
- [ ] stale 라인 번호/커버리지 주석 없음
- [ ] 코어 로직은 C++ 테스트로 작성
- [ ] 대응하는 `.ok` 파일 존재 (Tcl 테스트)
- [ ] 각 테스트 파일은 독립 실행 가능
