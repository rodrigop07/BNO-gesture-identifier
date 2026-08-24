# Controle de Volume por Gestos (BNO085 + Edge Impulse)

Este projeto implementa um modelo de **Machine Learning embarcado (TinyML)** para reconhecimento de gestos espaciais utilizando o sensor IMU **BNO085** e um microcontrolador da família **ESP32** (via ESP-IDF). O objetivo principal é permitir o controle de volume de um dispositivo realizando movimentos no ar com o braço, proporcionando uma interação natural sem contato físico.

## Funcionalidades

- **Reconhecimento de Gestos**: Identifica movimentos de "Círculo Horário" (Aumentar Volume), "Círculo Anti-horário" (Diminuir Volume) e "Repouso".
- **Inferência em Tempo Real**: O classificador roda 100% offline no dispositivo usando as bibliotecas do **Edge Impulse**, utilizando uma técnica de janela deslizante (*sliding window*) para alimentação contínua e sem atrasos perceptíveis.
- **Integração com Display OLED**: Fornece feedback visual em tempo real do nível de volume atual através de uma tela OLED onboard gerenciada pela biblioteca **LVGL**.
- **Leitura Estável de Sensores**: O BNO085 é lido a 100Hz (acelerômetro e giroscópio) de forma assíncrona. Os dados são protegidos contra condições de corrida (*race conditions*) utilizando Mutex (FreeRTOS).

## Tecnologias e Dependências

- **Microcontrolador:** Família ESP32 (ex: Heltec WiFi LoRa 32 V3 / ESP32-S3)
- **Sensor:** BNO085 (Comunicação I2C)
- **Framework:** [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/) (Testado na versão v6.1-beta1)
- **Bibliotecas e Componentes:**
  - **Edge Impulse SDK**: Processamento DSP e inferência da rede neural.
  - **LVGL**: Renderização de interface gráfica no display OLED.
  - **bno085**: Driver nativo para comunicação com o sensor.

## Estrutura do Projeto

- `/main/`: Código principal do projeto.
  - `main.cpp`: Lógica de *sliding window* e execução da rede neural.
  - `imu_config.c/h`: Inicialização do barramento I2C, configuração do BNO085 e captura dos dados com Mutex.
- `/components/`: Dependências locais de hardware e software.
  - `edge-impulse-sdk`, `model-parameters`, `tflite-model`: Modelo C++ exportado do Edge Impulse.
  - `oled_setup`, `oled_printf`, `i2c_config`: Utilitários para inicialização de vídeo e interface gráfica.
- `/managed_components/`: Componentes baixados automaticamente pelo IDF Component Manager (LVGL, driver do BNO085).

## Como Compilar e Rodar

1. **Configurar o ambiente ESP-IDF:**
   Certifique-se de ter o ESP-IDF instalado e devidamente exportado no seu terminal:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

2. **Ajustar os parâmetros (opcional):**
   Acesse o Menuconfig caso deseje alterar pinos do I2C ou partições de memória:
   ```bash
   idf.py menuconfig
   ```
   > **Nota:** Certifique-se de configurar a otimização de compilação (*Compiler Options*) para "Optimize for performance" para melhorar a velocidade da inferência.

3. **Compilar, Gravar e Monitorar:**
   Construa o projeto e grave no microcontrolador (substitua `/dev/ttyUSB0` pela sua porta serial):
   ```bash
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

##  Detalhes do Modelo (TinyML)

O modelo foi treinado no [Edge Impulse](https://edgeimpulse.com) utilizando a fusão dos dados brutos do acelerômetro e do giroscópio (eixos X, Y e Z).  
- **Tamanho da Janela (Window Size):** 1000ms
- **Incremento (Window Increase):** 250ms
- Foi utilizada uma classe `repouso` no dataset para atuar como ruído de fundo, impedindo que movimentos aleatórios ativem comandos não intencionais de volume.
- O limiar de confiança no código (`main.cpp`) está configurado para `0.6` (60%) de precisão para considerar a predição como um gesto válido.


---
*Este é um projeto acadêmico/técnico focado na aplicação de Machine Learning em Sistemas Embarcados.*