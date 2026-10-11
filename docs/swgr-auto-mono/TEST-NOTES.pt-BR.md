# Variante D — mono automático experimental

O teste da variante C confirmou, segundo o usuário, que copiar a imagem esquerda para os dois olhos resolve tanto introduções de fase quanto a animação de colisão/explosão. A variante D automatiza esse recurso por conteúdo da cena; não identifica semanticamente a cutscene nem corrige a renderização estéreo do jogo.

Teste sem F9. Começa em estéreo e sem mono manual forçado. Mantém o perfil atual e HDR desligado. Entra automaticamente em mono após duas amostras consecutivas com imagem à esquerda e preto à direita. Retorna ao estéreo após duas amostras com imagem à direita. Fades pretos nos dois olhos não ativam mono e encerram uma ativação anterior após três amostras escuras.

O detector lê 36 pontos de cada textura da cena antes da sobreposição do menu. Transfere os pontos para um buffer D3D12 READBACK, restaura os estados dos recursos e só lê na CPU depois que a fence terminou, sem esperar pela GPU no caminho normal. Intervalo mínimo de 40 ms entre coletas. A decisão exige várias observações e expira após 500 ms sem evidência fresca. A troca pode levar aproximadamente 80–150 ms conforme o ritmo dos frames e das transferências; precisa ser avaliada no headset.

Guardas: somente SWGR, D3D12, OpenXR, captura nativa não AFR e sem submissão em array, recursos BGRA compatíveis, mesma altura e largura por olho, uma camada/mip/amostra. Mantém as referências das texturas até a transferência terminar. Reinicialização do componente limpa recursos e evidência. Não altera gamma, resolução, controles ou perfil.

Logs: `[SWGRAutoMono][Sample]` registra quantos pontos têm algum canal RGB acima de 12/255; `[SWGRAutoMono][Transition]` registra a decisão. Ativa quando esquerda >= 4/36 e direita <= 1/36 por duas amostras; retorna quando direita >= 4/36 por duas amostras. Limites experimentais: cenas assimétricas muito escuras podem causar falso positivo; uma captura congelada com pixels não pretos pode escapar ao detector; não há garantia contra um primeiro frame preto.

F9 continua como força manual adicional. Começa desligado a cada nova execução do jogo. Desligar essa força não impede uma ativação automática justificada pelo detector. O teste de automatização deve ser feito sem pressionar F9. Para restaurar a variante C confirmada manualmente, feche o jogo e o injector e execute `testes-backend/03-C-cutscene-manual-F9.cmd`.

Referência de compatibilidade de formatos e estados das cópias: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion

Validação: os seis testes da suíte passaram, incluindo início em estéreo, rejeição de observação isolada, retorno ao estéreo, fades escuros, expiração de evidência e carregamento da política 4. A build e o teste no jogo devem ser registrados separadamente; testes de CPU não validam o comportamento da GPU ou a eficácia no headset.

Resultado posterior: build concluida e usuario confirmou em 11/10/2026 que a variante D automatica resolveu cutscenes e colisoes no Pico 4. Referencias a teste pendente acima descrevem o estado anterior a essa confirmacao.
