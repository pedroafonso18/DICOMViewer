# DICOM Viewer

Visualizador desktop de arquivos DICOM em desenvolvimento. A interface usa
Qt 6 Widgets e carrega a imagem em segundo plano com Qt Concurrent. O núcleo
C++20 encapsula o DCMTK, lê metadados e renderiza imagens monocromáticas.
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

## Testar

```sh
ctest --test-dir build --output-on-failure
```

Para configurar somente o núcleo e os testes, use
`-DDICOMVIEWER_BUILD_APP=OFF` na configuração do CMake.

A suíte unitária verifica as regras de Window Level e Window Width. A suíte de
integração usa DICOMs temporários para verificar a integridade dos pixels,
polaridade, transformações de contraste e leitura de frames nativos e JPEG
Lossless. O teste Qt executa sem servidor gráfico e verifica a proporção da
imagem e a preservação da visualização após uma falha de abertura. O workflow do
GitHub Actions executa as três suítes em Fedora.

Para incluir a mamografia do enunciado na validação, indique a cópia local:

```sh
DICOMVIEWER_REFERENCE_FILE="$HOME/Downloads/anonymized_mamo.dcm" \
  ctest --test-dir build -R '^dicom.integration$' --output-on-failure
```
