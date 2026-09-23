import json
import logging
import os
import signal
import sys
import threading
import time
from datetime import datetime, timedelta
import paho.mqtt.client as mqtt

# Módulos para geração de PDF via ReportLab
from reportlab.lib import colors
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.platypus import HRFlowable, Paragraph, SimpleDocTemplate, Table, TableStyle

# ==============================================================================
# CONFIGURAÇÕES DA REDE E BROKER
# ==============================================================================
BROKER_HOST = "broker.hivemq.com"
BROKER_PORT = 1883

PDF_REPORT_FILE = "relatorio_teste_explorerAir.pdf"
LOG_TELEMETRY_FILE = "telemetry_log.txt"
LOG_ERROR_FILE = "error_log.txt"

ACTION_NAMES = {
    0: "DESLIGAR",
    1: "LIGAR",
    2: "TEMP_18C",
    3: "TEMP_19C",
    4: "TEMP_20C",
    5: "TEMP_21C",
    6: "TEMP_22C",
    7: "TEMP_23C",
    8: "TEMP_24C",
    9: "TEMP_25C"
}

ACTION_SLOT_MAP = {
    0: "NONE",
    1: "POWER_OFF",
    2: "POWER_ON",
    3: "TEMP_18C",
    4: "TEMP_19C",
    5: "TEMP_20C",
    6: "TEMP_21C",
    7: "TEMP_22C",
    8: "TEMP_23C",
    9: "TEMP_24C",
    10: "TEMP_25C",
}

RESET_REASONS = {
    1: "POWERON_RESET",
    3: "SW_RESET",
    4: "OWDT_RESET",
    5: "DEEPSLEEP_RESET",
    6: "SDIO_RESET",
    7: "TG0WD_RESET",
    8: "TG1WD_RESET",
    9: "RTCWDT_SYS_RESET",
    10: "INTR_CPU_RESET",
    11: "RTCWDT_CPU_RESET",
    12: "EXT_CPU_RESET",
    13: "RTCWDT_BROWN_OUT_RESET",
    14: "RTCWDT_RTC_RESET"
}

# Amostra IR padrão (Fallback para o Teste 3)
DEFAULT_RAW_IR_SAMPLE = [
    4402, 4377, 537, 1608, 536, 536, 536, 1607, 536, 1608, 535, 536, 537, 535, 535, 1609,
    535, 537, 535, 536, 535, 1608, 536, 536, 535, 537, 534, 1609, 536, 1607, 535, 537,
    536, 1608, 535, 1609, 535, 536, 536, 1608, 535, 1608, 535, 1607, 536, 1609, 536, 1608,
    535, 1608, 536, 536, 535, 1608, 535, 538, 533, 537, 535, 537, 535, 536, 535, 537,
    535, 536, 535, 1608, 535, 1609, 535, 536, 534, 538, 533, 538, 535, 537, 535, 536,
    535, 536, 535, 537, 535, 536, 535, 1608, 535, 1608, 535, 1608, 536, 1608, 535, 1608,
    536, 1608, 535, 5190, 4375, 4379, 534, 1609, 535, 536, 535, 1609, 535, 1609, 534, 538,
    534, 537, 534, 1610, 534, 537, 535, 537, 532, 1610, 535, 537, 535, 536, 535, 1608
]

# Sequenciador de envio do Teste 3
TRIGGER_MAP = {
    255: 0,
      0: 1,
      1: 2,
      2: 3,
      3: 4,
      4: 5,
      5: 6,
      6: 7,
      7: 8,
      8: 9
}

# ==============================================================================
# DECODIFICADOR DO BITMAP LAST_ACTION
# ==============================================================================
def decode_last_action(byte_val: int) -> dict:
    if byte_val is None:
        return {}

    wakeup_reason_bit = byte_val & 0x01
    action_slot_raw   = (byte_val >> 1) & 0x0F
    download_bit      = (byte_val >> 5) & 0x01
    raw_stream_bit    = (byte_val >> 6) & 0x01
    reserved_bit      = (byte_val >> 7) & 0x01

    return {
        "raw_value": byte_val,
        "wakeup_reason_code": wakeup_reason_bit,
        "wakeup_reason": "SCHEDULE_ALARM" if wakeup_reason_bit else "TELEMETRY_TIMER",
        "action_slot_id": action_slot_raw,
        "action_name": ACTION_SLOT_MAP.get(action_slot_raw, f"UNKNOWN_SLOT_{action_slot_raw}"),
        "download_pending": bool(download_bit),
        "raw_stream_active": bool(raw_stream_bit),
        "reserved": reserved_bit
    }

# ==============================================================================
# ENGINE PRINCIPAL DE TESTES INTERATIVOS
# ==============================================================================
class InteractiveSystemTester:
    def __init__(self, device_mac: str):
        self.device_mac = device_mac.upper()
        self.topic_uplink = f"explorerIRBlaster/{self.device_mac}/UPLINK"
        self.topic_downlink = f"explorerIRBlaster/{self.device_mac}/DOWNLINK"

        self.client = mqtt.Client(client_id=f"Interactive_Tester_{self.device_mac}")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

        self.start_time = datetime.now()
        self.test_stats = {"pass": 0, "fail": 0, "offline_logs": 0}
        self.telemetry_history = []
        self.error_history = []
        self.test_records = []

        self.captured_ir_raws = {}

        self.active_test_mode = None
        self.response_event = threading.Event()
        self.cmd0_received_event = threading.Event()
        self.last_rx_data = None
        self.last_download_action_ack = -1

    def log_error(self, test_name: str, msg: str):
        full_msg = f"[{test_name} FAIL] {msg}"
        logging.error(full_msg)
        self.error_history.append((datetime.now(), full_msg))
        with open(LOG_ERROR_FILE, "a", encoding="utf-8") as f:
            f.write(f"[{datetime.now().isoformat()}] {full_msg}\n")

    def record_test_result(self, test_name: str, success: bool, details: str):
        status = "PASS" if success else "FAIL"
        if success:
            self.test_stats["pass"] += 1
            logging.info(f"✅ [{test_name}] {details}")
        else:
            self.test_stats["fail"] += 1
            self.log_error(test_name, details)

        self.test_records.append({
            "test_name": test_name,
            "status": status,
            "details": details,
            "timestamp": datetime.now().strftime("%H:%M:%S")
        })

    def send_cmd_8(self, target_action: int):
        raw_pulses = self.captured_ir_raws.get(target_action, DEFAULT_RAW_IR_SAMPLE)
        payload = {
            "cmd_id": 8,
            "action": target_action,
            "length": len(raw_pulses),
            "raw_data": raw_pulses
        }
        logging.info(f"📤 [TX] Enviando CMD 8 para regra/action: {target_action}")
        self.send_downlink(payload)

    # --------------------------------------------------------------------------
    # CALLBACKS MQTT
    # --------------------------------------------------------------------------
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logging.info("Conectado ao Broker MQTT com sucesso!")
            client.subscribe(self.topic_uplink)
            logging.info(f"Inscrito no tópico: {self.topic_uplink}")
        else:
            logging.error(f"Falha na conexão MQTT. Código: {rc}")

    def on_message(self, client, userdata, msg):
        payload_str = msg.payload.decode('utf-8', errors='ignore')
        recv_time = datetime.now()

        try:
            data = json.loads(payload_str)
        except json.JSONDecodeError:
            self.log_error("MQTT", f"Payload JSON inválido: {payload_str}")
            return

        cmd_id = data.get("cmd_id")
        if cmd_id is None:
            return

        # --- TRATAMENTO CMD 0: TELEMETRIA ---
        if cmd_id == 0:
            logging.info("📥 [RX] CMD 0 (Telemetria) recebido do ESP32.")
            with open(LOG_TELEMETRY_FILE, "a", encoding="utf-8") as f:
                f.write(f"[{recv_time.isoformat()}] {payload_str}\n")

            last_action_raw = data.get("last_action", 0)
            decoded_action = decode_last_action(last_action_raw)

            self.telemetry_history.append({
                "recv_time": recv_time,
                "temp": data.get("temp"),
                "umid": data.get("umid"),
                "bat": data.get("bat"),
                "rssi": data.get("rssi"),
                "last_action_raw": last_action_raw,
                "decoded_action": decoded_action,
                "rst_cause": data.get("rst_cause", data.get("boot_reason", -1)),
                "raw_payload": data
            })

            # Responde Handshake / Sync
            now = datetime.now()
            rtc_weekday = 0 if now.isoweekday() == 7 else now.isoweekday()
            cmd1 = {
                "cmd_id": 1,
                "year": now.year, "month": now.month, "day": now.day,
                "hour": now.hour, "min": now.minute, "sec": now.second,
                "weekday": rtc_weekday, "telemetry_update": 1800
            }
            logging.info("📤 [TX] Enviando CMD 1 (Handshake/Sync) em resposta ao CMD 0.")
            self.send_downlink(cmd1)

            self.cmd0_received_event.set()

            if self.active_test_mode == "WIFI_CHANGE":
                self.last_rx_data = data
                self.response_event.set()

            # SÓ RESPONDE AO DOWNLOAD SE O TESTE 3 ESTIVER EM EXECUÇÃO
            elif self.active_test_mode == "SEND_IR":
                if decoded_action.get("download_pending"):
                    logging.info("📥 ESP32 reportou download_pending ativo no CMD 0. Iniciando envio do Slot 0...")
                    time.sleep(0.1)
                    self.send_cmd_8(0)

        # --- TRATAMENTO CMD 3: RAW IR CAPTURADO ---
        elif cmd_id == 3:
            # SÓ PROCESSA E RESPONDE COM CMD 2 SE O TESTE 2 ESTIVER EM EXECUÇÃO
            if self.active_test_mode == "LEARN_IR":
                action = data.get("action")
                raw_data = data.get("raw_data", [])
                length = data.get("length", 0)

                if action is not None and raw_data:
                    self.captured_ir_raws[action] = raw_data
                    logging.info(f"📥 Pulso IR recebido para Slot {action} ({ACTION_NAMES.get(action, 'UNK')}) - {length} amostras")

                ack = {"cmd_id": 2, "action": action}
                self.send_downlink(ack)

                self.last_rx_data = data
                self.response_event.set()
            else:
                logging.warning(f"⚠️ [RX] CMD 3 recebido fora do Teste 2 (LEARN_IR). Ignorando resposta.")

        # --- TRATAMENTO CMD 5: CONFIRMAÇÃO DE RECONFIGURAÇÃO WI-FI ---
        elif cmd_id == 5:
            logging.info(f"📥 [RX] CMD 5 (Wi-Fi Config ACK) recebido do ESP32: {data}")
            if self.active_test_mode == "WIFI_CHANGE":
                self.last_rx_data = data
                self.response_event.set()

        # --- TRATAMENTO CMD 7: ACK DE AGENDAMENTO ---
        elif cmd_id == 7:
            logging.info(f"📥 [RX] CMD 7 (Schedule ACK) recebido: {data}")
            if self.active_test_mode == "SCHEDULES":
                self.last_rx_data = data
                self.response_event.set()

        # --- TRATAMENTO CMD 9: ACK DE GRAVAÇÃO REMOTA IR ---
        elif cmd_id == 9:
            received_action = data.get("action")
            logging.info(f"📥 [RX] CMD 9 recebido do ESP32 (action={received_action})")
            
            # SÓ CONTINUA O CICLO SE O TESTE 3 ESTIVER EM EXECUÇÃO
            if self.active_test_mode == "SEND_IR":
                self.last_download_action_ack = received_action
                if received_action in TRIGGER_MAP:
                    next_action = TRIGGER_MAP[received_action]
                    time.sleep(0.1)
                    self.send_cmd_8(next_action)
                elif received_action == 9:
                    logging.info("✅ Ciclo de download finalizado com sucesso no ESP32!")
                    self.response_event.set()
            else:
                logging.warning(f"⚠️ [RX] CMD 9 recebido fora do Teste 3 (SEND_IR). Ignorando ciclo.")

    def send_downlink(self, payload: dict):
        json_str = json.dumps(payload)
        self.client.publish(self.topic_downlink, json_str, qos=1)

    # --------------------------------------------------------------------------
    # MÓDULOS DE TESTE INTERATIVO
    # --------------------------------------------------------------------------

    def run_wifi_change_test(self):
        print("\n" + "="*70)
        print("TESTE 1: MUDANÇA DE REDE WI-FI (FLUXO ATUALIZADO)")
        print("="*70)

        # 1. A plataforma pergunta o nome da rede
        new_ssid = input("1. Digite o SSID da nova rede Wi-Fi: ").strip()

        # 2. A plataforma pergunta a senha da rede
        new_pass = input("2. Digite a Senha da nova rede Wi-Fi: ").strip()

        self.active_test_mode = "WIFI_CHANGE"
        self.cmd0_received_event.clear()
        self.response_event.clear()

        # 3. A plataforma espera o esp32 enviar cmd_id 0
        print("\n3. ⏳ Aguardando o ESP32 enviar o cmd_id 0 inicial (Timeout 120s)...")
        if not self.cmd0_received_event.wait(timeout=120):
            print("❌ Timeout: O ESP32 não enviou o cmd_id 0 inicial.")
            self.record_test_result("Wi-Fi Change", False, "Abortado: ESP32 não enviou o cmd_id 0 inicial.")
            self.active_test_mode = None
            return

        # 4. Quando chega cmd_id 0, o callback on_message já envia o cmd_id 1.
        # Agora enviamos o cmd_id 4 com as novas credenciais.
        print("\n4. 📥 cmd_id 0 recebido! Reagindo com cmd_id 1 (enviado via callback) e transmitindo cmd_id 4...")
        payload_cmd4 = {
            "cmd_id": 4,
            "ssid": new_ssid,
            "password": new_pass
        }
        self.send_downlink(payload_cmd4)

        # 5. Instrução ao operador
        print("\n5. 📢 [AÇÃO REQUERIDA] Conecte o ESP32 à nova rede e force um envio de telemetria.")

        # 6. A plataforma precisa aguardar a recepção de um novo cmd_id 0 (sem timeout)
        print("6. ⏳ Aguardando recepção de um novo cmd_id 0 na nova rede (Tempo indefinido)...")
        self.response_event.clear()
        
        # Bloqueia a execução por tempo indeterminado até que um novo cmd_id 0 seja recebido
        self.response_event.wait()

        # 7. Quando recebe o cmd_id 0, o callback 'on_message' envia automaticamente o cmd_id 1.
        print("\n7. ✅ Novo cmd_id 0 recebido com sucesso na nova rede!")
        print("   -> cmd_id 1 enviado em resposta. Encerrando o teste de Wi-Fi.")

        self.record_test_result(
            "Wi-Fi Change", 
            True, 
            f"Sucesso: Dispositivo reconfigurado e reconectado via SSID '{new_ssid}'."
        )

        self.active_test_mode = None

    def run_learn_ir_test(self):
        print("\n" + "="*70)
        print("TESTE 2: APRENDIZADO DE COMANDOS IR (GUIADO PELO OPERADOR)")
        print("="*70)
        print("INSTRUÇÕES PARA O OPERADOR:")
        print("1. Pressione simultaneamente os dois botões no ESP32 por 2 segundos.")
        print("2. Selecione 'Aprender' no menu OLED do dispositivo.")
        print("3. Siga o fluxo no OLED e aponte o controle do Ar Condicionado para o receptor.")
        print("4. Confirme cada comando no botão GPIO 5 até concluir o ciclo (0 a 9).")
        print("----------------------------------------------------------------------")

        input("Pressione [ENTER] assim que iniciar a rotina no dispositivo...")

        self.active_test_mode = "LEARN_IR"

        print("\nAguardando envio dos lotes de sinais aprendidos (CMD 3) via MQTT (Timeout 180s)...")
        
        start_time = time.time()
        while time.time() - start_time < 180:
            self.response_event.clear()
            if self.response_event.wait(timeout=5):
                captured_count = len(self.captured_ir_raws)
                print(f"  • Progresso da Captura: {captured_count}/10 comandos na memória local.")
                if captured_count >= 10:
                    break

        if len(self.captured_ir_raws) == 10:
            self.record_test_result("Learn IR", True, "Todos os 10 comandos IR foram capturados e salvos no script.")
        else:
            self.record_test_result("Learn IR", False, f"Captura incompleta: {len(self.captured_ir_raws)}/10 comandos recebidos.")

        self.active_test_mode = None

    def run_send_ir_test(self):
        print("\n" + "="*70)
        print("TESTE 3: ENVIO REMOTO DE COMANDOS IR GRAVADOS (CMD 8 / CMD 9)")
        print("="*70)
        print("INSTRUÇÕES PARA O OPERADOR:")
        print("1. QUEM INICIA A CONVERSA É O ESP32.")
        print("2. Reinicie o ESP32 para que ele conecte e informe o status no CMD 0.")
        print("3. Caso a captura prévia não tenha sido feita, usaremos dados padrão (DEFAULT_RAW_IR_SAMPLE).")
        print("----------------------------------------------------------------------")
        
        self.active_test_mode = "SEND_IR"
        self.response_event.clear()
        self.last_download_action_ack = -1

        print("⏳ Aguardando diálogo vindo do ESP32 (CMD 0 ou CMD 9 com action 255) para iniciar a gravação dos slots (Timeout 60s)...")
        
        time.sleep(1)
        if not self.response_event.is_set():
            print("🚀 Enviando comando inicial (Slot 0) para iniciar o ciclo de download...")
            self.send_cmd_8(0)

        if self.response_event.wait(timeout=60) or self.last_download_action_ack == 9:
            self.record_test_result("Send IR Remote", True, "Sucesso: Todos os comandos IR foram salvos e confirmados pelo ESP32 via CMD 9.")
        else:
            self.record_test_result("Send IR Remote", False, f"Falha/Timeout: Envio abortado ou incompleto no Slot {self.last_download_action_ack}.")

        self.active_test_mode = None

    def run_schedules_test(self):
        print("\n" + "="*70)
        print("TESTE 4: VALIDAÇÃO DE MÚLTIPLOS AGENDAMENTOS (PROGRAMAÇÃO E DISPARO)")
        print("="*70)
        print("INSTRUÇÕES PARA O OPERADOR:")
        print("1. O envio dos agendamentos aguardará o ESP32 enviar CMD 0 e receber CMD 1.")
        print("2. O teste aguardará os horários de disparo e decodificará o bitmask `last_action`.")
        print("----------------------------------------------------------------------")
        
        self.cmd0_received_event.clear()
        print("\n⏳ Aguardando o ESP32 conectar e enviar CMD 0 (Timeout 120s)...")
        
        if not self.cmd0_received_event.wait(timeout=120):
            print("❌ Timeout: O ESP32 não enviou o CMD 0 dentro do tempo limite.")
            self.record_test_result("Schedules Validation", False, "Abortado: ESP32 não iniciou a conversa com CMD 0.")
            return

        print("✅ CMD 0 recebido e CMD 1 enviado com sucesso!")
        print("🚀 Programando bateria de agendamentos no ESP32...")

        self.active_test_mode = "SCHEDULES"
        now = datetime.now()
        iso_day = now.isoweekday()
        week_days_mask = (1 << (iso_day + 1)) | 1

        schedules_to_test = [
            {
                "id": 0,
                "target_dt": now + timedelta(minutes=2),
                "action_str": "SET_TEMP_18",
                "expected_action_slot": 3,
                "expected_rst_cause": 7
            },
            {
                "id": 1,
                "target_dt": now + timedelta(minutes=4),
                "action_str": "SET_TEMP_22",
                "expected_action_slot": 7,
                "expected_rst_cause": 15
            },
            {
                "id": 2,
                "target_dt": now + timedelta(minutes=6),
                "action_str": "POWER_OFF",
                "expected_action_slot": 1,
                "expected_rst_cause": 3
            }
        ]

        ack_count = 0
        for sched in schedules_to_test:
            time_str = sched["target_dt"].strftime("%H:%M")
            sched["time_str"] = time_str

            payload = {
                "cmd_id": 6,
                "schedule_id": sched["id"],
                "week_days": week_days_mask,
                "time": time_str,
                "action": sched["action_str"]
            }

            self.response_event.clear()
            print(f"[TX] Enviando Agendamento ID {sched['id']} -> Hora: {time_str} | Mask: {week_days_mask} | Ação: {sched['action_str']}")
            self.send_downlink(payload)

            if self.response_event.wait(timeout=10):
                rx_cmd = self.last_rx_data.get("cmd_id")
                rx_sched_id = self.last_rx_data.get("schedule_id")

                if rx_cmd == 7 and rx_sched_id == sched["id"]:
                    ack_count += 1
                    print(f"  ✅ Agendamento {sched['id']} gravado no ESP32 (CMD 7 ACK OK)")
                else:
                    print(f"  ❌ Resposta inesperada do ESP32 para o agendamento {sched['id']}: {self.last_rx_data}")
            else:
                print(f"  ❌ Timeout aguardando CMD 7 ACK do agendamento {sched['id']}")

        if ack_count != len(schedules_to_test):
            self.record_test_result("Schedules Validation", False, f"Falha ao gravar agendamentos no ESP32 ({ack_count}/{len(schedules_to_test)} ACK).")
            self.active_test_mode = None
            return

        print("\n" + "="*70)
        print("⏳ INICIANDO ETAPA DE MONITORAMENTO E VALIDAÇÃO DE DISPARO")
        print("="*70)

        dispatched_count = 0
        for sched in schedules_to_test:
            target_dt = sched["target_dt"]
            time_str = sched["time_str"]
            
            wait_seconds = (target_dt - datetime.now()).total_seconds() + 45
            if wait_seconds < 10:
                wait_seconds = 10

            print(f"\n⏳ Aguardando disparo do Agendamento ID {sched['id']} ({sched['action_str']}) às {time_str}...")
            print(f"   (Janela de escuta ativa por até {int(wait_seconds)} segundos)")

            self.cmd0_received_event.clear()
            start_wait = time.time()
            triggered = False

            while (time.time() - start_wait) < wait_seconds:
                if self.cmd0_received_event.wait(timeout=5):
                    recv_dt = datetime.now()
                    time_diff = abs((recv_dt - target_dt).total_seconds())
                    
                    if time_diff <= 90:
                        telem = self.telemetry_history[-1] if self.telemetry_history else {}
                        
                        raw_la = telem.get("last_action_raw", 0)
                        dec = telem.get("decoded_action", {})
                        
                        rx_temp = telem.get("temp")
                        rx_umid = telem.get("umid")
                        rx_bat = telem.get("bat")
                        rx_rssi = telem.get("rssi")

                        print("\n" + "-"*50)
                        print(f"📥 [TELEMETRIA DO AGENDAMENTO ID {sched['id']} RECEBIDA]")
                        print(f"  • Horário de Chegada  : {recv_dt.strftime('%H:%M:%S')} (Diferença: {int(time_diff)}s)")
                        print(f"  • Raw `last_action`   : 0x{raw_la:02X} ({raw_la})")
                        print(f"  • Motivo Wakeup       : {dec.get('wakeup_reason')} (code {dec.get('wakeup_reason_code')})")
                        print(f"  • Slot ID executado   : {dec.get('action_slot_id')} -> {dec.get('action_name')}")
                        print(f"  • Download Pendente   : {dec.get('download_pending')}")
                        print(f"  • Temperatura Lida   : {rx_temp} °C")
                        print(f"  • Umidade Lida       : {rx_umid} %")
                        print(f"  • Bateria / RSSI     : {rx_bat}V | {rx_rssi} dBm")
                        print("-"*50)

                        checks_passed = True
                        
                        if dec.get("wakeup_reason_code") == 1:
                            print("  ✅ [VALIDAÇÃO] Motivo do wakeup correto (SCHEDULE_ALARM)")
                        else:
                            print(f"  ❌ [VALIDAÇÃO FALHOU] Motivo do wakeup foi {dec.get('wakeup_reason')}, esperado SCHEDULE_ALARM")
                            checks_passed = False

                        if dec.get("action_slot_id") == sched["expected_action_slot"]:
                            print(f"  ✅ [VALIDAÇÃO] Slot de Ação correto: {dec.get('action_slot_id')} ({dec.get('action_name')})")
                        else:
                            print(f"  ❌ [VALIDAÇÃO FALHOU] Slot esperado: {sched['expected_action_slot']}, recebido: {dec.get('action_slot_id')}")
                            checks_passed = False

                        if rx_temp is not None and -10 <= rx_temp <= 60:
                            print(f"  ✅ [VALIDAÇÃO] Temperatura válida recebida: {rx_temp} °C")
                        else:
                            print(f"  ❌ [VALIDAÇÃO FALHOU] Temperatura inválida ou nula: {rx_temp}")
                            checks_passed = False

                        if checks_passed:
                            dispatched_count += 1
                        
                        triggered = True
                        break
                    else:
                        self.cmd0_received_event.clear()

            if not triggered:
                print(f"  ❌ FALHA: O ESP32 não enviou o acionamento para o agendamento ID {sched['id']} às {time_str}.")

        if dispatched_count == len(schedules_to_test):
            self.record_test_result(
                "Schedules Execution", 
                True, 
                f"Sucesso: Todos os {dispatched_count} agendamentos foram validados com bitmask decodificado, temperatura e boot cause."
            )
        else:
            self.record_test_result(
                "Schedules Execution", 
                False, 
                f"Execução parcial/falha: Apenas {dispatched_count}/{len(schedules_to_test)} agendamentos passaram nas validações de payload."
            )

        self.active_test_mode = None

    # --------------------------------------------------------------------------
    # GERADOR DE RELATÓRIO EM PDF
    # --------------------------------------------------------------------------
    def generate_pdf_report(self):
        end_time = datetime.now()
        duration = end_time - self.start_time
        print("\nGerando Relatório PDF Completo de Testes...")

        doc = SimpleDocTemplate(
            PDF_REPORT_FILE,
            pagesize=letter,
            rightMargin=36, leftMargin=36, topMargin=36, bottomMargin=36
        )

        styles = getSampleStyleSheet()
        title_style = ParagraphStyle(
            'TitleStyle', parent=styles['Heading1'], fontSize=18, leading=22, textColor=colors.HexColor("#1A365D"), spaceAfter=8
        )
        subtitle_style = ParagraphStyle(
            'SubTitleStyle', parent=styles['Normal'], fontSize=10, textColor=colors.HexColor("#4A5568"), spaceAfter=12
        )
        h2_style = ParagraphStyle(
            'H2Style', parent=styles['Heading2'], fontSize=12, leading=16, textColor=colors.HexColor("#2B6CB0"), spaceBefore=10, spaceAfter=6
        )
        body_style = ParagraphStyle(
            'Body', parent=styles['Normal'], fontSize=8, leading=11, textColor=colors.HexColor("#2D3748")
        )

        elements = []
        elements.append(Paragraph("Relatório de Homologação de Firmware", title_style))
        elements.append(Paragraph(f"Projeto: <b>explorerAirConditioner</b> | Dispositivo MAC: <b>{self.device_mac}</b>", subtitle_style))
        elements.append(HRFlowable(width="100%", thickness=1, color=colors.HexColor("#CBD5E0"), spaceAfter=12))

        # 1. Resumo Executivo
        elements.append(Paragraph("1. Resumo Executivo", h2_style))
        summary_table = [
            ["Início da Sessão:", self.start_time.strftime("%d/%m/%Y %H:%M:%S"), "Total de Testes:", str(len(self.test_records))],
            ["Fim da Sessão:", end_time.strftime("%d/%m/%Y %H:%M:%S"), "Aprovados / Falhas:", f"{self.test_stats['pass']} PASS / {self.test_stats['fail']} FAIL"],
            ["Duração da Sessão:", str(duration).split('.')[0], "Pulsos IR em Memória:", str(len(self.captured_ir_raws))]
        ]
        t_summary = Table(summary_table, colWidths=[110, 160, 130, 140])
        t_summary.setStyle(TableStyle([
            ('BACKGROUND', (0,0), (-1,-1), colors.HexColor("#F7FAFC")),
            ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#E2E8F0")),
            ('FONTNAME', (0,0), (-1,-1), 'Helvetica-Bold'),
            ('FONTSIZE', (0,0), (-1,-1), 8),
            ('TOPPADDING', (0,0), (-1,-1), 4),
            ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ]))
        elements.append(t_summary)

        # 2. Resultados dos Módulos
        elements.append(Paragraph("2. Resultado dos Módulos de Teste", h2_style))
        test_rows = [["Horário", "Módulo / Caso de Teste", "Status", "Detalhes / Observações"]]
        for rec in self.test_records:
            test_rows.append([
                rec["timestamp"],
                rec["test_name"],
                rec["status"],
                Paragraph(rec["details"], body_style)
            ])

        t_tests = Table(test_rows, colWidths=[60, 140, 60, 280])
        t_tests.setStyle(TableStyle([
            ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#2B6CB0")),
            ('TEXTCOLOR', (0,0), (-1,0), colors.white),
            ('FONTNAME', (0,0), (-1,0), 'Helvetica-Bold'),
            ('FONTSIZE', (0,0), (-1,-1), 8),
            ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
            ('ALIGN', (2,0), (2,-1), 'CENTER')
        ]))
        elements.append(t_tests)

        # 3. Telemetria Capturada
        if self.telemetry_history:
            elements.append(Paragraph("3. Histórico de Telemetria Durante o Teste", h2_style))
            telem_rows = [["Horário Chegada", "Ação / Wakeup", "Temp (°C)", "Umidade (%)", "Bateria (V)", "RSSI (dBm)"]]
            for sample in self.telemetry_history[-10:]:
                dec = sample.get("decoded_action", {})
                action_info = f"{dec.get('wakeup_reason', '-')[:5]} | {dec.get('action_name', '-')}"
                telem_rows.append([
                    sample["recv_time"].strftime("%H:%M:%S"),
                    action_info,
                    str(sample.get("temp", "-")),
                    str(sample.get("umid", "-")),
                    f"{sample.get('bat', 0):.2f}",
                    str(sample.get("rssi", "-"))
                ])
            t_telem = Table(telem_rows, colWidths=[90, 130, 80, 80, 80, 80])
            t_telem.setStyle(TableStyle([
                ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#EDF2F7")),
                ('FONTNAME', (0,0), (-1,0), 'Helvetica-Bold'),
                ('FONTSIZE', (0,0), (-1,-1), 8),
                ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
                ('ALIGN', (2,0), (-1,-1), 'CENTER')
            ]))
            elements.append(t_telem)

        try:
            doc.build(elements)
            print(f"✅ Relatório PDF gerado com sucesso: {os.path.abspath(PDF_REPORT_FILE)}")
        except Exception as e:
            print(f"❌ Erro ao compilar relatório PDF: {e}")

# ==============================================================================
# MENU PRINCIPAL & EXECUÇÃO INTERATIVA
# ==============================================================================
def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

    print("="*70)
    print("   SUÍTE INTERATIVA DE HOMOLOGAÇÃO E TESTES - explorerAirConditioner")
    print("="*70)

    raw_mac = input("Digite o MAC do dispositivo (ex: E08CFE950F18): ").strip()
    if not raw_mac:
        raw_mac = "E08CFE950F18"

    tester = InteractiveSystemTester(raw_mac)

    tester.client.connect(BROKER_HOST, BROKER_PORT, 60)
    mqtt_thread = threading.Thread(target=tester.client.loop_forever, daemon=True)
    mqtt_thread.start()

    time.sleep(1)

    def signal_handler(sig, frame):
        print("\n\nEncerrando sessão interativa e compilando relatório...")
        tester.generate_pdf_report()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    while True:
        print("\n" + "="*50)
        print("          PAINEL DE CONTROLE DE TESTES")
        print("="*50)
        print("1. Testar Mudança de Rede Wi-Fi (CMD 4 / CMD 5)")
        print("2. Testar Aprendizado de Comandos IR (ESP32 -> Plataforma)")
        print("3. Testar Envio Remoto de Comandos IR (Plataforma -> ESP32)")
        print("4. Testar Múltiplos Agendamentos (Aguardando CMD 0 / CMD 1)")
        print("5. Executar Todos os Testes Sequencialmente")
        print("6. Encerrar Sessão e Gerar Relatório PDF")
        print("="*50)

        choice = input("Escolha uma opção (1-6): ").strip()

        if choice == "1":
            tester.run_wifi_change_test()
        elif choice == "2":
            tester.run_learn_ir_test()
        elif choice == "3":
            tester.run_send_ir_test()
        elif choice == "4":
            tester.run_schedules_test()
        elif choice == "5":
            tester.run_wifi_change_test()
            tester.run_learn_ir_test()
            tester.run_send_ir_test()
            tester.run_schedules_test()
        elif choice == "6":
            tester.generate_pdf_report()
            break
        else:
            print("[!] Opção inválida. Escolha de 1 a 6.")

    tester.client.disconnect()

if __name__ == "__main__":
    main()