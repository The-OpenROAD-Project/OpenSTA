# TODO4 - Modified `*/test` Files Review

검토 대상(`git status` 기준):
- liberty/test/cpp/CMakeLists.txt
- search/test/cpp/CMakeLists.txt
- liberty/test/cpp/TestLibertyClasses.cc
- liberty/test/cpp/TestLibertyStaBasics.cc
- liberty/test/cpp/TestLibertyStaCallbacks.cc
- search/test/cpp/TestSearchClasses.cc
- search/test/cpp/TestSearchStaDesign.cc
- search/test/cpp/TestSearchStaInit.cc
- search/test/cpp/TestSearchStaInitB.cc
- (deleted) liberty/test/cpp/TestLiberty.cc
- (deleted) search/test/cpp/TestSearch.cc

정량 요약(신규/분할된 cpp 테스트 파일):
- `TestLibertyClasses.cc`: 320 tests, `<5 checker` 278
- `TestLibertyStaBasics.cc`: 596 tests, `<5 checker` 533
- `TestLibertyStaCallbacks.cc`: 125 tests, `<5 checker` 116
- `TestSearchClasses.cc`: 225 tests, `<5 checker` 215
- `TestSearchStaDesign.cc`: 655 tests, `<5 checker` 653
- `TestSearchStaInit.cc`: 629 tests, `<5 checker` 610
- `TestSearchStaInitB.cc`: 623 tests, `<5 checker` 621, `SUCCEED()` 47

필수 요구 반영:
- 짧은 테스트(의미 없는 단일 checker) 보강: 최소 5개 이상의 checker를 갖는 유의미 시나리오 테스트로 재작성
- `SUCCEED()` 전면 제거: 모두 삭제하고 최소 5개 이상의 실질 checker로 대체

TODO - liberty/test/cpp/CMakeLists.txt:20# 기존 단일 타겟(`TestLiberty`) 삭제로 기존 빌드/CI 스크립트 호환성 리스크가 있음. 호환 alias 타겟 유지 또는 마이그레이션 가이드 필요.
TODO - search/test/cpp/CMakeLists.txt:20# 기존 단일 타겟(`TestSearch`) 삭제로 기존 빌드/CI 스크립트 호환성 리스크가 있음. 호환 alias 타겟 유지 또는 마이그레이션 가이드 필요.

TODO - liberty/test/cpp/TestLibertyClasses.cc:47# 단일 수치/포인터 확인 수준의 짧은 테스트가 대량(320개 중 278개 `<5 checker`) 존재. API 단위 검사 대신 경계값/상호작용 포함 5+ checker 시나리오로 통합 필요.
TODO - liberty/test/cpp/TestLibertyClasses.cc:91# `EXPECT_NE(..., nullptr)` 1개로 끝나는 존재성 테스트 다수. null 이외 상태(단위, suffix, 변환 round-trip, opposite/alias 일관성)까지 검증하도록 보강 필요.

TODO - liberty/test/cpp/TestLibertyStaBasics.cc:98# 단일 존재성 테스트가 대량(596개 중 533개 `<5 checker`). fixture 기반 실제 동작 검증(입력-출력-불변식)으로 재작성 필요.
TODO - liberty/test/cpp/TestLibertyStaBasics.cc:216# `ASSERT_NO_THROW` 블록에서 결과 상태를 검증하지 않는 패턴 다수. 호출 성공 외 최소 5개 의미 있는 post-condition 검증 추가 필요.
TODO - liberty/test/cpp/TestLibertyStaBasics.cc:5160# `EXPECT_TRUE(true)`는 무의미 테스트. 제거 후 실제 결과(멤버 탐색 성공/실패, 경계 인덱스, 반복자 개수, 이름 일관성 등) 5+ checker로 대체 필요.

TODO - liberty/test/cpp/TestLibertyStaCallbacks.cc:89# `/tmp` 고정 경로 기반 임시 파일 생성은 병렬 실행/권한/충돌 리스크. 테스트 전용 temp helper(고유 디렉토리+자동 정리)로 교체 필요.
TODO - liberty/test/cpp/TestLibertyStaCallbacks.cc:100# `fopen` 실패 시 조용히 return 하여 원인 은닉. 즉시 ASSERT/FAIL로 실패 원인 표면화 필요.
TODO - liberty/test/cpp/TestLibertyStaCallbacks.cc:113# helper 내부에서 `EXPECT_NE(lib, nullptr)`만 수행하면 호출 테스트가 단일 checker로 축소됨. 파싱된 라이브러리의 핵심 속성 5+ checker 검증으로 승격 필요.
TODO - liberty/test/cpp/TestLibertyStaCallbacks.cc:135# callback 커버리지 테스트 다수가 `ASSERT_NO_THROW` 위주(125개 중 116개 `<5 checker`). callback 결과 side-effect(값 반영, 객체 상태, 에러 경로)를 다중 checker로 검증 필요.

TODO - search/test/cpp/TestSearchClasses.cc:165# 기본 생성자/이동 생성자 테스트가 타입 값 1~2개 확인에 치중(225개 중 215개 `<5 checker`). 복합 타입 전이/대입 후 불변식까지 5+ checker 보강 필요.
TODO - search/test/cpp/TestSearchClasses.cc:196# `PropertyValue pv(3.14f, nullptr)`는 주석상 잠재 segfault 경로. 유효 Unit fixture 사용 및 문자열 변환/복사/이동 안정성까지 검증 필요.

TODO - search/test/cpp/TestSearchStaDesign.cc:158# 반환값을 `(void)`로 버리는 테스트 패턴 다수(655개 중 653개 `<5 checker`). 실제 timing 수치/관계/정렬/개수에 대한 5+ checker 검증으로 전환 필요.
TODO - search/test/cpp/TestSearchStaDesign.cc:6403# SDC write 테스트가 파일 open 성공만 확인(단일 checker). 파일 내용 키워드/명령 수/제약 반영 여부를 최소 5개 checker로 검증 필요.
TODO - search/test/cpp/TestSearchStaDesign.cc:6404# `/tmp` 하드코딩 출력 파일명은 병렬 테스트 충돌 위험. test result helper 기반 고유 파일 경로 사용 필요.
TODO - search/test/cpp/TestSearchStaDesign.cc:6457# `ASSERT_NO_THROW` 중심 테스트 다수에서 post-condition 부재. 생성된 path/end/group 결과의 정합성 체크 추가 필요.

TODO - search/test/cpp/TestSearchStaInit.cc:93# 컴포넌트 존재성 단일 테스트가 과다(629개 중 610개 `<5 checker`). fixture 초기화 불변식 묶음(네트워크/코너/리포터/스레드/namespace) 5+ checker로 통합 필요.
TODO - search/test/cpp/TestSearchStaInit.cc:2473# 함수 주소 존재성 테스트(`auto fn = ...; EXPECT_NE`)는 실행 의미가 없음. 실사용 경로에서 동작/예외/상태 변화를 검증하는 시나리오로 대체 필요.
TODO - search/test/cpp/TestSearchStaInit.cc:2489# `ASSERT_NO_THROW` 후 검증이 없는 no-op 테스트. 호출 전후 상태 차이 및 부수효과를 5+ checker로 확인 필요.

TODO - search/test/cpp/TestSearchStaInitB.cc:87# 단일 EXPECT_THROW/EXPECT_NE 중심 초단문 테스트가 과다(623개 중 621개 `<5 checker`). 기능 단위 묶음으로 재설계해 의미 있는 5+ checker 검증 필요.
TODO - search/test/cpp/TestSearchStaInitB.cc:237# 함수 포인터 존재성 테스트가 대량(weak fn-pointer tests 253개). 컴파일 타임 보장은 별도 정적 검증으로 넘기고, 런타임 시나리오 테스트로 대체 필요.
TODO - search/test/cpp/TestSearchStaInitB.cc:305# `ASSERT_NO_THROW` 호출-only 패턴이 반복되어 회귀 탐지력이 낮음. 호출 결과/상태 전이/예외 경계까지 포함한 5+ checker 보강 필요.
TODO - search/test/cpp/TestSearchStaInitB.cc:1656# `ClockPinPairLessExists`는 `SUCCEED()`로 종료되는 무의미 테스트. 실제 비교 규칙(반사성/반대칭/일관성/동치) 5+ checker로 대체 필요.

TODO - liberty/test/cpp/TestLiberty.cc:1# 파일 삭제로 인해 기존 통합 회귀 시나리오가 분할 파일에 완전 이관됐는지 증빙 부재. 고가치 시나리오(파서+STA 연동)의 회귀 추적표 필요.
TODO - search/test/cpp/TestSearch.cc:1# 파일 삭제로 인해 기존 통합 검색/타이밍 회귀 시나리오 이관 검증이 필요. 테스트 명 매핑표와 누락 케이스 점검 필요.

# SUCCEED() 전면 제거 TODO (요청사항 2 필수 반영)
TODO - search/test/cpp/TestSearchStaInitB.cc:1661# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2609# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2620# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2631# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2896# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2906# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2916# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:2994# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3004# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3014# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3076# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3103# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3472# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3502# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3553# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3565# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3584# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3603# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3670# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3698# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3708# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3720# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3730# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3755# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3890# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:3899# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4053# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4062# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4073# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4084# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4095# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4106# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4117# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4148# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4183# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4192# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4203# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4214# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4251# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4412# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4431# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4442# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4453# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4572# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4583# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4812# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
TODO - search/test/cpp/TestSearchStaInitB.cc:4823# SUCCEED() 제거 필요. 단순 통과 표시 대신 최소 5개 이상의 의미 있는 checker(결과 값/상태/부수효과/예외 경계)로 대체.
