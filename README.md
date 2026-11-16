# DICOM Viewer

Visualizador desktop de imagens DICOM. A interface usa
Qt 6 Widgets e carrega a imagem em segundo plano com Qt Concurrent. O núcleo
C++20 encapsula o DCMTK, lê metadados e renderiza imagens monocromáticas, RGB,
PALETTE COLOR, YBR_FULL, YBR_FULL_422 e YBR_PARTIAL_422. O leitor inclui
decodificadores JPEG, JPEG-LS e RLE Lossless.
A imagem abre na orientação original, ajustada à área de visualização com a
proporção indicada pelo DICOM. O núcleo usa a janela ou a VOI LUT gravada no
arquivo quando disponível.

## Compilar e executar

Ambiente alvo: Fedora Linux 45 Workstation (x86_64), com Qt 6.11.2 e DCMTK 3.6.9.

```sh
sudo dnf install cmake gcc-c++ make qt6-qtbase-devel dcmtk-devel gtest-devel gmock
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/dicom_viewer
```

Selecione **Open…** ou pressione **Ctrl+O** para abrir um arquivo DICOM. Se
a abertura falhar, a mensagem identifica o arquivo e a imagem anterior
continua disponível.

## Controles do visualizador

| Controle | Comportamento |
| --- | --- |
| Roda do mouse e arraste | Amplia em torno do cursor e desloca a imagem. |
| **Rotate Left** / **Rotate Right** | Gira a imagem em passos de 90°. |
| **Fit to Window** | Centraliza e reenquadra a imagem com a rotação atual. |
| **Restore View** | Volta ao primeiro frame, à orientação, ao enquadramento e ao contraste da abertura. |
| **Previous** / **Next** | Troca de frame preservando enquadramento, rotação e contraste manual. |
| **Preset**, **Window Level** e **Window Width** | Selecionam a janela gravada ou ajustam o contraste em imagens monocromáticas. Para substituir uma VOI LUT gravada, selecione **Custom**. |
| **Metadata** | Abre ou recolhe o painel lateral de metadados. |

A interface acompanha o tema do sistema e a área da imagem tem fundo escuro.
O suporte RGB foi validado com imagens nativas intercaladas e planares. A
leitura multiframe foi validada com arquivos nativos, JPEG Lossless e
PALETTE COLOR com compressão RLE Lossless.

## Testar

```sh
ctest --test-dir build --output-on-failure
```

Para configurar somente o núcleo e os testes, use
`-DDICOMVIEWER_BUILD_APP=OFF` na configuração do CMake.

A suíte unitária verifica as regras de Window Level e Window Width. A suíte de
integração usa DICOMs temporários para verificar a integridade dos pixels,
polaridade, transformações de contraste, ordem dos canais RGB e leitura de
frames nativos e JPEG Lossless. Os testes Qt executam sem servidor gráfico e
verificam abertura com erro, navegação, contraste e transformação do viewport.
O workflow do GitHub Actions executa as três suítes em Fedora.

Para incluir a mamografia do enunciado na validação, indique a cópia local:

```sh
DICOMVIEWER_REFERENCE_FILE="$HOME/Downloads/anonymized_mamo.dcm" \
  ctest --test-dir build -R '^dicom.integration$' --output-on-failure
```
