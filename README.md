# DICOM Viewer

[Read in English](README.en.md)

Visualizador desktop de arquivos DICOM em C++20 e Qt 6 Widgets. Abra um arquivo local para ver a imagem na orientação original e com a proporção indicada pelo DICOM. A decodificação e a renderização ocorrem em segundo plano. Se a abertura falhar, a interface identifica o arquivo, oferece detalhes do erro e mantém a imagem anterior disponível.

## Compilar no Linux

É necessário um compilador com suporte a C++20, CMake 3.24 ou superior, DCMTK 3.6.9 ou superior, Qt 6 com Widgets e Concurrent, e GoogleTest para os testes.

### Fedora Linux 45

```sh
sudo dnf install git cmake gcc-c++ make qt6-qtbase-devel dcmtk-devel gtest-devel gmock
```

O ambiente de validação usa Qt 6.11.2 e DCMTK 3.6.9.

### Ubuntu 26.04 LTS

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  git build-essential cmake qt6-base-dev qt6-qpa-plugins \
  libdcmtk-dev libjpeg-dev libpng-dev libtiff-dev libgtest-dev libgmock-dev
```

O ambiente de validação usa Qt 6.10.2. Os pacotes de desenvolvimento de JPEG, PNG e TIFF fornecem bibliotecas usadas pelos alvos CMake exportados pelo DCMTK.

### Compilar e executar

Os comandos a seguir servem para as duas distribuições. Se você já clonou o repositório, comece pelo primeiro comando `cmake` na raiz do projeto.

```sh
git clone https://github.com/pedroafonso18/DICOMViewer.git
cd DICOMViewer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/dicom_viewer
```

## Usar o visualizador

| Controle | Comportamento |
| --- | --- |
| **Open…** ou **Ctrl+O** | Abre o seletor para arquivos `.dcm` e `.DCM`. |
| Roda do mouse e arraste | Amplia em torno do cursor e desloca a imagem. |
| **Rotate Left** / **Rotate Right** | Gira a imagem em passos de 90°. |
| **Fit to Window** | Centraliza e reenquadra a imagem mantendo a rotação atual. |
| **Restore View** | Volta ao primeiro frame e restaura a orientação, o enquadramento e o contraste iniciais. |
| **Previous** / **Next** | Troca de frame mantendo rotação, zoom, deslocamento e contraste manual. |
| **Preset**, **Window Level** e **Window Width** | Selecionam uma janela gravada ou ajustam o contraste de imagens monocromáticas. |
| **Metadata** | Mostra ou recolhe o painel lateral com dimensões, espaçamento de pixel, orientação, lateralidade e sintaxe de transferência, quando disponíveis. |

**File default** usa a janela gravada para o frame ou a tabela de contraste (VOI LUT) do arquivo, quando disponível. Na ausência de uma transformação aplicável, o visualizador calcula uma janela automática. Selecione **Custom** para substituir a VOI LUT por um ajuste manual. A interface acompanha o tema do sistema e mantém o fundo escuro na área da imagem.

## Compatibilidade DICOM

- **Fotometria** — `MONOCHROME1`, `MONOCHROME2`, `RGB`, `PALETTE COLOR`, `YBR_FULL`, `YBR_FULL_422` e `YBR_PARTIAL_422`. Imagens coloridas são exibidas em RGB.
- **Codificação** — dados de pixel nativos, JPEG convencional (incluindo Baseline e Lossless), JPEG-LS e RLE Lossless.
- **Estudos multiframe** — navegação dentro do arquivo aberto, com decodificação de frames sob demanda. Quando necessário, o núcleo percorre sequencialmente frames JPEG fragmentados com tabela de offsets vazia.

JPEG 2000, HTJ2K, JPEG XL, vídeo encapsulado e dados de pixel de ponto flutuante estão fora do suporte atual. O visualizador abre um arquivo por vez. A navegação entre arquivos de uma série, a reconstrução de volumes e o acesso a PACS estão fora do escopo.

## Estrutura do projeto

| Local | Responsabilidade |
| --- | --- |
| [`src/dicomviewer/`](src/dicomviewer/) | Núcleo C++ independente do Qt. Valida o arquivo, mantém a sessão DCMTK, aplica contraste, devolve pixels e metadados e relata falhas com erros tipados. |
| [`src/viewer/`](src/viewer/) | Interface Qt Widgets. O carregador usa Qt Concurrent e converte os pixels em `QImage`. O viewport cuida do zoom, do deslocamento e da rotação. |
| [`tests/`](tests/) | Suítes GoogleTest executadas pelo CTest. |
| [`.github/workflows/build.yml`](.github/workflows/build.yml) | Jobs de build e testes em Fedora 45, Ubuntu 26.04 LTS e Windows Server 2025 (x64). |

No Linux, as leituras posteriores continuam vinculadas ao arquivo que a sessão abriu. A interface aplica apenas o resultado da solicitação mais recente de frame ou contraste.

O job do Windows usa MSVC 2022, Qt 6.8.3 e os pacotes DCMTK e GoogleTest do vcpkg. Os ambientes Fedora e Ubuntu usam os pacotes de suas distribuições.

## Testar

```sh
ctest --test-dir build --output-on-failure
```

| Suíte CTest | Cobertura |
| --- | --- |
| `dicom.unit` | Validação de Window Level e Window Width. |
| `dicom.integration` | Leitura com DCMTK, erros de arquivo, polaridade, VOI LUT, janelas por frame, canais de cor e navegação em arquivos nativos ou comprimidos. |
| `dicom.viewer` | Interface Qt em modo offscreen, incluindo falha de abertura, contraste, navegação, zoom, rotação e restauração. |

Os testes geram seus próprios DICOMs. Para incluir a mamografia de referência do enunciado, passe o caminho da cópia local. Esse caso é ignorado quando a variável estiver ausente.

```sh
DICOMVIEWER_REFERENCE_FILE="$HOME/Downloads/anonymized_mamo.dcm" \
  ctest --test-dir build -R '^dicom\.integration$' --output-on-failure
```

Para compilar apenas o núcleo e suas suítes, use um diretório de build separado e desative a interface Qt.

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DDICOMVIEWER_BUILD_APP=OFF
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```
