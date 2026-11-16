# DICOM Viewer

Visualizador desktop de arquivos DICOM em desenvolvimento. O núcleo C++20
encapsula o DCMTK e pode ser compilado e testado independentemente da interface
Qt 6 Widgets. Ele lê os metadados, renderiza imagens monocromáticas e retorna
erros tipados para que a interface possa exibir mensagens específicas.

## Compilar e testar o núcleo

Ambiente alvo: Fedora Linux 45 Workstation (x86_64).

```sh
sudo dnf install cmake gcc-c++ make dcmtk-devel gtest-devel gmock
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A suíte unitária verifica as regras de Window Level e Window Width. A suíte de
integração cria arquivos DICOM em um diretório temporário para verificar a
leitura, a polaridade, a ordem dos pixels, a janela, o JPEG Lossless e os erros de
abertura. O workflow do GitHub Actions executa as duas suítes em Fedora.

Para incluir a mamografia do enunciado na validação, indique a cópia local:

```sh
DICOMVIEWER_REFERENCE_FILE="$HOME/Downloads/anonymized_mamo.dcm" \
  ctest --test-dir build -R '^dicom.integration$' --output-on-failure
```
