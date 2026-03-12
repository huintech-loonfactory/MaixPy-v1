# lcd_mcu ST7735S Unit Test (inside MaixPy-v1)

Host-side C unit test for `components/drivers/lcd/src/lcd_mcu.c` with mocked LCD/SPI hooks.

## Scope
- ST7735S pixel format branch (`0x05`)
- ST7789 pixel format regression (`0x55`)
- ST7735S pre-init command sequence timing (before `SLEEP_OFF`)
- Pre-init handler isolation across consecutive init calls

## Location
- `device/MaixPy-v1/tests/c_sdk_unit`

## Run (PowerShell)
```powershell
cd device/MaixPy-v1/tests/c_sdk_unit
./run_test.ps1
```

## Run (WSL/Linux)
```bash
cd device/MaixPy-v1/tests/c_sdk_unit
bash ./run_test.sh
```

## Run (VSCode Testing)
- VSCode `Testing` 뷰에서 `test_lcd_mcu_st7735s_host_unit` 실행
- 테스트는 내부적으로 `run_test.ps1`(Windows) 또는 `run_test.sh`(WSL/Linux)를 호출
- `unittest` discovery 기반으로 등록되어 있어 별도 `pytest` 설치가 필요 없음

Requires `gcc` or `clang` on PATH.
