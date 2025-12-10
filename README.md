# Experimento NS-3: Rede Wi-Fi IoT

Professor: Fabio Rocha  
Alunos: Pedro Belchior; Antonio Neto

## Objetivo
Desenvolver um experimento no NS-3 para comparar o desempenho de diferentes configuracoes de rede sem fio, avaliando como parametros fisicos e de camada MAC impactam a qualidade da comunicacao.

## Visao geral do cenario (`telecom.cc`)
- 1 no coletor atua como ponto de acesso (AP).
- Entre 20 e 40 sensores IoT atuam como estacoes (STA), posicionados aleatoriamente em uma area quadrada (por padrao 100 m x 100 m).
- Canal Wi-Fi com modelo de perda LogDistance; a potencia de transmissao e o expoente do modelo podem ser ajustados.
- Pilha IP completa e aplicacoes UDP: sensores enviam trafego para o coletor.
- FlowMonitor coleta metricas de desempenho ao final da simulacao.

## Parametros configuraveis (linha de comando)
- `--nSensors` (default: 30): quantidade de sensores IoT (intervalo recomendado: 20 a 40).
- `--simTime` (30.0): tempo total de simulacao em segundos.
- `--txPower` (16.0): potencia de transmissao em dBm.
- `--packetInterval` (1.0): intervalo entre pacotes enviados por cada sensor (s).
- `--packetSize` (40): tamanho do pacote em bytes.
- `--areaSize` (100.0): lado do quadrado onde os nos sao posicionados (m).
- `--pathLossExp` (3.0): expoente do modelo LogDistance para perda de percurso.

## Como executar (exemplo)
1) Garanta que o NS-3 esteja configurado e que `telecom.cc` esteja acessivel pelo `waf` (por exemplo em `scratch/`).
2) Compile: `./waf` (ou `./waf build` dependendo do ambiente).
3) Rode a simulacao ajustando os parametros de interesse, por exemplo:
```
./waf --run "telecom --nSensors=30 --simTime=30 --txPower=16 --packetInterval=1 --packetSize=40 --areaSize=100 --pathLossExp=3"
```

## Metricas exibidas na saida
- Pacotes transmitidos e recebidos (servidor UDP no coletor).
- PDR (% de pacotes recebidos sobre transmitidos).
- Atraso medio fim-a-fim (s).
- Throughput medio (kbps).

## Dicas para comparacoes
- Varie potencia (`--txPower`) e expoente de perda (`--pathLossExp`) para observar impacto de propagacao.
- Ajuste intervalo de envio (`--packetInterval`) e tamanho de pacote para estudar saturacao do canal.
- Mude densidade de sensores (`--nSensors`) e area (`--areaSize`) para analisar efeitos de concorrencia e distancia.
- Registre os resultados impressos em diferentes execucoes para comparar cenarios.
