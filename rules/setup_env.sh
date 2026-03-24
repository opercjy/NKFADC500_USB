#!/bin/bash
# setup_env.sh

# 스크립트 자신의 절대 경로를 자동으로 추적 (어디서 실행하든 에러 방지)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
RULE_FILE="$SCRIPT_DIR/99-kfadc500.rules"

echo -e "\033[1;36m[SYSTEM] Notice KFADC500 DAQ Environment Setup\033[0m"

# 1. 규칙 파일 복사
if [ -f "$RULE_FILE" ]; then
    echo "[INFO] Found rule file: $RULE_FILE"
    echo "[INFO] Copying udev rules to /etc/udev/rules.d/ ..."
    sudo cp "$RULE_FILE" /etc/udev/rules.d/
else
    echo -e "\033[1;31m[ERROR] Rule file not found at: $RULE_FILE\033[0m"
    exit 1
fi

# 2. Udev 데몬 리로드
echo "[INFO] Reloading udev daemon..."
sudo udevadm control --reload-rules
sudo udevadm trigger

echo -e "\033[1;32m[SUCCESS] USB permissions configured. You can now run the DAQ without sudo.\033[0m"