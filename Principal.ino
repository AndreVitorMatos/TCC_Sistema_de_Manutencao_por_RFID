// Importando bibliotecas
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <MFRC522.h>
#include <SPI.h>
// Definindo constantes de pinos e buffers do RC522
#define SS_PIN 21
#define RST_PIN 22
#define SIZE_BUFFER 18
#define MAX_SIZE_BLOCK 16
// Configurando RFID
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
MFRC522 mfrc522(SS_PIN, RST_PIN);
// Configurando Wifi
const char *ssid = "Sistema de Manutencao";
const char *password = "123456789";
WiFiServer server(80);
/* Definindo variáveis globais, sendo elas:
  DadosOperador(booleano) -> Indica ao sistema se os dados do operador foram preenchidos ao menos uma vez.
  DadosRealizarManutencao(booleano) -> Indica ao sistema se os dados de manutenção foram preenchidos,
    permitindo a gravação das informações na etiqueta.
  TextoPOST(String) -> Recebe todas as informações enviadas pelo método POST.
  NomeOperador(String) -> Recebe o nome do operador que está utilizando o sistema.
  MatriculaOperador(String) -> Recebe a matrícula do operador que está utilizando o sistema.
  TipoOperacao(String) -> Recebe a informação do tipo de manutenção que foi realizada no equipamento.
  ItensSubstituidos(String) -> Recebe a informação dos itens substituídos durante a manutenção.
  ItensAvariados(String) -> Recebe a informação dos itens que apresentaram avaria durante a manutenção.
  Horario(String) -> Recebe todas as informações de data em que ocorreu a manutenção.
  OBS(String) -> Recebe o texto de observações informados pelo operador, essa informação não será 
    gravada na etiqueta, somente apresentado no comprovante.
  Tag(String) -> Recebe todas as informações lidas da etiqueta.
  IDTag(String) -> Recebe o ID único da etiqueta lida.
  valortag(Vetor de 16 caracteres) -> Armazena as informações de leitura da etiqueta, armazenando 
    após a leitura de cada bloco na variável Tag.
  TipoComprovante(inteiro) -> Indica a tela de comprovante qual deve ser o tipo de comprovante gerado, valendo:
    0: erro.
    1: leitura.
    2: manutenção.
  buffer(Vetor de 16 bytes) -> Recebe inicialmente os dados da etiqueta.
  */
boolean DadosOperador = false;
boolean DadosRealizarManutencao = false;
String TextoPOST, NomeOperador, MatriculaOperador, TipoOperacao, ItensSubstituidos, ItensAvariados, Horario, OBS, Tag, IDTag;
char valortag[MAX_SIZE_BLOCK];
int TipoComprovante = 0;
byte buffer[MAX_SIZE_BLOCK] = "";

void setup() {
  /*Função setup(), responsável pela inicialização do sistema.*/
  //Inicialização da serial para verificação dos processos ao ser conectado com um computador
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");
  //Inicialização do WiFi, configuração da rede como ponto de acesso e definição do IP, Gateway e subnet
  IPAddress staticIP(192, 168, 4, 2);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  if (!WiFi.softAP(ssid, password)) {
    log_e("Soft AP creation failed.");
    while (1)
      ;
  }
  WiFi.config(staticIP, gateway, subnet);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();
  Serial.println("Server started");
  //Inicialização da comunicação SPI e do modulo RC522
  SPI.begin();
  mfrc522.PCD_Init();
}
void loop() {
  /*Função loop():
    Objetivo: Esta função é a principal do sistema, e deve rodar infinitamente os comandos dentro dela...
      O principal objetivo é verificar a conexão de um dispositivo ao sistema, e caso tenha conexão realizar
      o tratamento do requerimento HTTP e o redirecionamento para tela solicitada.
  */
  WiFiClient client = server.available();
  //Verifica se exite alguma conexão com um dispositivo.
  if (!client) {
    return;
  }
  Serial.println("New Client.");
  //Espera o requerimento chegar.
  while (!client.available()) {
    delay(1);
  }
  //Lê o requerimento HTTP
  String req = client.readStringUntil('\r');
  Serial.println(req);
  //Verificando se existe dados via POST
  if (req.indexOf("POST") != -1) {
    TextoPOST = "";
    while (client.available()) {
      String line = client.readStringUntil('\r');
      //Verifica se existe dados do formulário de dados do operador
      if(line.indexOf("Nome") !=-1){
        TextoPOST = line.substring(line.indexOf("Nome"));
        Serial.print("texto: " + TextoPOST);
      }
      //Verifica se existe dados do formulário de manutenção
      if(line.indexOf("Tipo") !=-1){
        TextoPOST = line.substring(line.indexOf("Tipo"));
        Serial.print("texto: " + TextoPOST);
      }
    }
  }
  //Quebra o requerimento HTTP, salvando somente a parte que será utilizada
  req = req.substring(req.indexOf("/") + 1, req.indexOf("HTTP") - 1);
  Serial.println(req);
  /*Trata o requerimento HTTP da seguinte forma
    Caso não tenha nenhuma informação ou o texto "TelaInicial" -> Chama a tela inicial.
    Caso apresente o texto "DadosOperador" -> Chama a tela para preenchimento dos dados do operador.
    Caso apresente o texto "RealizarOperacao" -> Chama a tela para escolha da operação que deve ser realizada.
    Caso apresente o texto "LerManutencao" ->  Chama a tela para leitura de uma etiqueta para a operação ler manutenção.
    Caso apresente o texto "Comprovante" ->  Chama a tela para geração do comprovante de leitura ou manutenção.
    Caso apresente o texto "RealizarManutencao" -> Chama a tela para leitura de uma etiqueta para a operação realizar manutenção.
    Caso apresente o texto "FormularioManutencao" -> Chama a tela para preenchimento dos dados de manutenção.
    Caso apresente o texto "GravarTag" ->  Chama a tela para gravação da etiqueta.
    Caso apresente nenhuma das opções acima -> Chama uma tela de erro.
  */
  if (req.length() == 0 || req.indexOf("TelaInicial") != -1) {
    TelaInicial(client);
  } else if (req.indexOf("DadosOperador") != -1) {
    //Verifica se existem informações via POST
    if (TextoPOST.indexOf("Nome") != -1) {
      //Separa as informações e realiza o tratamento.
      if (TextoPOST.substring(TextoPOST.indexOf("Nome=") + 5, TextoPOST.indexOf("&")) != "") {
        NomeOperador = TextoPOST.substring(TextoPOST.indexOf("Nome=") + 5, TextoPOST.indexOf("&"));
        NomeOperador.replace("%23", " ");
        NomeOperador = DecodeURL(NomeOperador);
      }
      if (TextoPOST.substring(TextoPOST.indexOf("Matricula=") + 10) != "") {
        MatriculaOperador = TextoPOST.substring(TextoPOST.indexOf("Matricula=") + 10);
        MatriculaOperador.replace("%23", " ");
        MatriculaOperador = DecodeURL(MatriculaOperador);
      }
      //Somente troca o valor da variável DadosOperador caso tanto o nome como a matricula estiverem preenchidas
      if (NomeOperador.length() != 0 && MatriculaOperador != 0) {
        DadosOperador = true;
      }
    }
    //Limpa variável TextoPOST
    TextoPOST = "";
    TelaDadosOperador(client);
  } else if (req.indexOf("RealizarOperacao") != -1) {
    //Chama LimpaVariaveis, para que uma operação anterior não interfira em uma nova operação
    LimpaVariaveis();
    TelaDefinirOperacao(client);
  } else if (req.indexOf("LerManutencao") != -1) {
    //O parâmetro false é utilizado para identificar que a leitura vem da operação ler manutenção
    TelaLerManutencao(client, false);
  } else if (req.indexOf("Comprovante") != -1) {
    TelaComprovante(client);
  } else if (req.indexOf("RealizarManutencao") != -1) {
    //O parâmetro true é utilizado para identificar que a leitura vem da operação realizar manutenção
    TelaLerManutencao(client, true);
  } else if (req.indexOf("FormularioManutencao") != -1) {
    //Verifica se existem informações via POST
    if (TextoPOST.indexOf("Tipo") != -1) {
      //Separa as informações e realiza o tratamento.
      if (TextoPOST.substring(TextoPOST.indexOf("Tipo=") + 5, TextoPOST.indexOf("&")) != "") {
        TipoOperacao = TextoPOST.substring(TextoPOST.indexOf("Tipo=") + 5, TextoPOST.indexOf("&"));
        TipoOperacao.replace("%23", " ");
        TipoOperacao = DecodeURL(TipoOperacao);
      }
      if (TextoPOST.substring(TextoPOST.indexOf("ItensS=") + 7, TextoPOST.indexOf("&ItensA=")) != "") {
        ItensSubstituidos = TextoPOST.substring(TextoPOST.indexOf("ItensS=") + 7, TextoPOST.indexOf("&ItensA="));
        ItensSubstituidos.replace("%23", " ");
        ItensSubstituidos = DecodeURL(ItensSubstituidos);
      }
      if (TextoPOST.substring(TextoPOST.indexOf("ItensA=") + 7, TextoPOST.indexOf("&Horario=")) != "") {
        ItensAvariados = TextoPOST.substring(TextoPOST.indexOf("ItensA=") + 7, TextoPOST.indexOf("&Horario="));
        ItensAvariados.replace("%23", " ");
        ItensAvariados = DecodeURL(ItensAvariados);
      }
      if (TextoPOST.substring(TextoPOST.indexOf("Horario=") + 8) != "") {
        Horario = TextoPOST.substring(TextoPOST.indexOf("Horario=") + 8, TextoPOST.indexOf("&Obs="));
        Horario.replace("T", " ");
        Horario.replace("%3A", ":");
      }
      if (TextoPOST.substring(TextoPOST.indexOf("Obs=") + 4) != "") {
        OBS = TextoPOST.substring(TextoPOST.indexOf("Obs=") + 4);
        OBS.replace("%23", " ");
        OBS = DecodeURL(OBS);
      }
      //Limpa variável TextoPOST
      TextoPOST = "";
      DadosRealizarManutencao = true;
    }
    FormularioManutencao(client);
  } else if (req.indexOf("GravarTag") != -1) {
    TelaGravarTag(client);
  } else {
    client.println("Requisicao Invalida");
  }
  //Realiza procedimento para encerrar a comunicação HTTP e se preparar para uma nova requisição
  client.flush();
  client.stop();
  //Finaliza os processos do modulo RC522
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
void TelaInicial(WiFiClient client) {
  /*Função TelaInicial(WiFiClient client)
    Objetivo:
      Construir a tela inicial do sistema.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  //Constrói o cabeçalho e o css da pagina
  Cabecalho(client);
  //Apresenta os dados e o botão de realizar operação caso os dados do operador já estiverem preenchidos
  if (DadosOperador == true) {
    DadosOperadorPreechido(client);
    client.println("<a href=\"RealizarOpera&ccedil;&atilde;o\"><button>Realizar Operacao</button></a>");
  }
  client.println("<a href=\"DadosOperador\"><button>Preencher dados do operador</button></a>");
  client.println("</body>");
  client.println("</html>");
  client.println();
}
void TelaDadosOperador(WiFiClient client) {
  /*Função TelaDadosOperador(WiFiClient client)
    Objetivo:
      Construir a tela de formulário para preenchimento dos dados do operador.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  //Constrói o cabeçalho e o css da pagina
  Cabecalho(client);
  //Apresenta os dados do operador caso já estiverem preenchidos
  if (DadosOperador == true) {
    DadosOperadorPreechido(client);
  }
  //Constrói formulário dos dados do operador
  client.println("<form method=\"POST\">");
  client.println("<h2>Dados do operador</h2><input type=\"text\" name=\"Nome\"/>");
  client.println("<h2>Matr&iacute;cula</h2><input type=\"text\" name=\"Matricula\">");
  client.println("<input type=\"submit\" id=\"meu-submit\" value=\"Enviar\" /></form>");
  client.println("<a href=\"TelaInicial\"><button>Voltar</button></a>");
  client.println("</body>");
  client.println();
}
void TelaDefinirOperacao(WiFiClient client) {
  /*Função TelaDefinirOperacao(WiFiClient client)
    Objetivo:
      Construir a tela de menu para escolha da operação que será realizada
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  //Constrói a tela caso os dados do operador estejam preenchidos, caso contrário constrói uma tela de erro
  if (DadosOperador == true) {
    //Constrói o cabeçalho e o css da pagina
    Cabecalho(client);
    //Apresenta os dados do operador
    DadosOperadorPreechido(client);
    //Constrói menu
    client.println("<a href='LerManutencao'><button>Ler Manuten&ccedil;&atilde;o</button></a>");
    client.println("<a href='RealizarManutencao'><button>Realizar Manuten&ccedil;&atilde;o</button></a>");
    client.println("<a href='TelaInicial'><button>Voltar</button></a>");
    client.println("</body>");
    client.println();
  } else {
    //Constrói o cabeçalho e o css da pagina
    Cabecalho(client);
    //Constrói mensagem de erro e botão de voltar
    client.println("<h2>Preencha os dados de operador!!</h2>");
    client.println("<a href='TelaInicial'><button>Voltar</button></a>");
    client.println("</body>");
  }
}
void TelaLerManutencao(WiFiClient client, boolean manutencao) {
  /*Função TelaLerManutencao(WiFiClient client, boolean manutencao)
    Objetivo:
      Construir a tela de leitura e chamar a função responsável pela leitura.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
      boolean manutencao -> Informa se a tela foi chamada na operação de leitura ou de realizar manutenção.
        true : realizar manutenção
        false : leitura da última manutenção.
    Retorna: 
      Nada.
  */
  //Constrói a tela caso os dados do operador estejam preenchidos, caso contrário constrói uma tela de erro
  if (DadosOperador == true) {
    //Constrói o cabeçalho e o css da página.
    Cabecalho(client);
    //Apresenta os dados do operador.
    DadosOperadorPreechido(client);
    //Solicita a aproximação da etiqueta e chama a função LerTag.
    client.println("<p>Aproxime o equipamento da etiqueta...</p>");
    LerTag();
    //A partir a construção da tela só vai ocorrer após a leitura da etiqueta.
    client.println("<p>Etiqueta Lida !!!</p>");
    //Cria o botão da próxima página de acordo com o valor da variável manutenção
    if (manutencao == false) {
      client.println("<a href='Comprovante'><button>Gerar Comprovante</button></a)");
      TipoComprovante = 1;
    } else {
      client.println("<a href='FormularioManutencao'><button>Preencher formulario</button></a)");
    }
    client.println("</body>");
    client.println();
  } else {
    //Constrói o cabeçalho e o css da página.
    Cabecalho(client);
    //Constrói mensagem de erro e botão de voltar.
    client.println("<h1>Sistema de manuten&ccedil;&atilde;o</h1>");
    client.println("<h2>Preencha os dados de operador!!</h2>");
    client.println("<a href='TelaInicial'><button>Voltar</button></a>");
  }
}
void TelaComprovante(WiFiClient client) {
  /*Função TelaComprovante(WiFiClient client)
    Objetivo:
      Construir a tela de comprovante de leitura ou de gravação.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  //Constrói o cabeçalho e o css da página.
  Cabecalho(client);
  /*Identifica por meio da variável global TipoComprovante se o comprovante é de leitura ou de gravação:
    TipoComprovante == 1: Comprovante de leitura.
    TipoComprovante == 2: Comprovante de gravação.
  */
  if (TipoComprovante == 1) {
    client.println("<h2>Comprovante de leitura de manuten&ccedil;&atilde;o</h2>");
    //Apresenta o horário que o comprovante foi gerado com auxílio do código javascript no final da página.
    client.println("<p id=\"horario\">");
    //Apresenta os dados do responsável pela leitura.
    client.println("<p>Operador respons&aacute;vel pela leitura:</p>");
    client.println("<p>Nome: " + NomeOperador + "</p>");
    client.println("<p>Matr&iacute;cula: " + MatriculaOperador + "</p>");
    client.println("<p>Dados do equipamento: </p>");
    //Apresenta os dados do equipamento e da última manutenção.
    client.println("<p>ID:" + IDTag + "</p>");
    client.println("<p>Dados de manuten&ccedil;&atilde;o: </p>");
    client.println("<p>Operador: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Matr&iacute;cula do Operador: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Tipo de Manuten&ccedil;&atilde;o: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Itens com avaria: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Itens substituidos: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Hor&aacute;rio: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = "";
    TipoComprovante = 0;
  } else if (TipoComprovante == 2) {
    client.println("<h2>Comprovante de grava&ccedil;&atilde;o de manuten&ccedil;&atilde;o</h2>");
    //Apresenta o horário que o comprovante foi gerado com auxílio do código javascript no final da página.
    client.println("<p id=\"horario\">");
    //Apresenta os dados do responsável pela leitura.
    client.println("<p>Operador responsavel pela leitura:</p>");
    client.println("<p>Nome: " + NomeOperador + "</p>");
    client.println("<p>Matr&iacute;cula: " + MatriculaOperador + "</p>");
    client.println("<h3>Manuten&ccedil;&atilde;o anterior</h3>");
    //Apresenta os dados do equipamento e da última manutenção antes da gravação.
    client.println("<p>Dados do equipamento: </p>");
    client.println("<p>ID:" + IDTag + "</p>");
    client.println("<p>Dados de manuten&ccedil;&atilde;o: </p>");
    client.println("<p>Operador: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Matr&iacute;cula do Operador: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Tipo de Manuten&ccedil;&atilde;o: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Itens com avaria: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Itens substituidos: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = Tag.substring(Tag.indexOf("#") + 1);
    client.println("<p>Hor&aacute;rio: " + Tag.substring(0, Tag.indexOf("#")) + "</p>");
    Tag = "";
    TipoComprovante = 0;
    //Apresenta os dados da última manutenção após a gravação.
    client.println("<h3>Dados da nova manuten&ccedil;&atilde;o</h3>");
    client.println("<p>Operador responsavel pela gravacao:</p>");
    client.println("<p>Nome: " + NomeOperador + "</p>");
    client.println("<p>Matr&iacute;cula: " + MatriculaOperador + "</p>");
    client.println("<p>Dados da manuten&ccedil;&atilde;o: </p>");
    client.println("<p>Tipo: " + TipoOperacao + "</p>");
    client.println("<p>Itens Substituidos: " + ItensSubstituidos + "</p>");
    client.println("<p>Itens Avariados: " + ItensAvariados + "</p>");
    client.println("<p>Hor&aacute;rio da manuten&ccedil;&atilde;o: " + Horario + "</p>");
    client.println("<p>Observa&ccedil;&otilde;es: " + OBS + "</p>");
    TipoComprovante = 0;
  }
  //Cria botão para imprimir, que utiliza o javascript no final da página.
  client.println("<button id='imprimir'>Imprimir</button>");
  client.println("<a href='RealizarOperacao'><button>Voltar</button></a>");
  //Inicio do javascript.
  client.println("<script>");
  //Criando o comportamento de imprimir a tela para o botão de imprimir.
  client.println("const btn_imprimir = document.getElementById(\"imprimir\")");
  client.println("btn_imprimir.addEventListener(\"click\",(evt)=>{window.print()})");
  //Criando o comportamento de apresentar o horário na tela
  client.println("function mostrarHorario() {");
  client.println("var dataAtual = new Date();");
  client.println("var hora = dataAtual.getHours();");
  client.println("var minuto = dataAtual.getMinutes();");
  client.println("var segundo = dataAtual.getSeconds();");
  client.println("var dia = dataAtual.getDate();");
  client.println("var mes = dataAtual.getMonth() + 1;");
  client.println("var ano = dataAtual.getFullYear();");
  client.println("var horaAtualString = dia +'-'+ mes + '-' + ano + ' as ' + hora + ':' + minuto + ':' + segundo;");
  client.println("document.getElementById(\"horario\").innerText = \"Este comprovante foi gerado na data: \" + horaAtualString;");
  client.println("}");
  client.println("mostrarHorario()");
  client.println("</script>");
  client.println("</body>");
}
void FormularioManutencao(WiFiClient client) {
  /*Função FormularioManutencao(WiFiClient client)
    Objetivo:
      Construir a tela de formulário de manutenção para gravar na etiqueta
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  //Constrói o cabeçalho e o css da página.
  Cabecalho(client);
  //Constrói a tela caso os dados do operador estejam preenchidos, caso contrário constrói uma tela de erro.
  if (DadosOperador == true) {
    DadosOperadorPreechido(client);
    //Apresenta os dados do formulário para gravação caso o formulário já tenha sido preenchido ao menos uma vez.
    if (DadosRealizarManutencao == true) {
      client.println("<p>Dados da manuten&ccedil;&atilde;o preechido:</p>");
      client.println("<p>Tipo: " + TipoOperacao + "</p>");
      client.println("<p>Itens Substituidos: " + ItensSubstituidos + "</p>");
      client.println("<p>Itens Avariados: " + ItensAvariados + "</p>");
      client.println("<p>Horario da manuten&ccedil;&atilde;o: " + Horario + "</p>");
      client.println("<p>Observa&ccedil;&otilde;es: " + OBS + "</p>");
      client.println("<a href='GravarTag'><button>Gravar na etiqueta</button></a>");
      client.println("<h2>Modifique os dados de manuten&ccedil;&atilde;o</h2>");
    } else {
      client.println("<h2>Insira os dados de manuten&ccedil;&atilde;o</h2>");
    }
    //Constrói o formulário.
    client.println("<form method=\"POST\">");
    client.println("<h3>Tipo de manuten&ccedil;&atilde;o</h3><input type='text' name='Tipo' placeholder='Tipo de Manuten&ccedil;&atilde;o'/>");
    client.println("<h3>Itens Substituidos(cod. separados por espa&ccedil;o)</h3><input type='text' name='ItensS' placeholder='Itens Substituidos'>");
    client.println("<h3>Itens com Avaria(cod. separados por espa&ccedil;o)</h3><input type='text' name='ItensA' placeholder='Itens com Avaria'>");
    client.println("<h3>Dia e hor&aacuterio de manuten&ccedil;&atilde;o</h3><input type='datetime-local' name='Horario'>");
    client.println("<h3>Observa&ccedil;&otilde;es</h3><textarea name='Obs'></textarea>");
    client.println("<input type='submit' id='meu-submit' value='Enviar' /></form>");
    client.println("<a href='RealizarOperacao'><button>Voltar</button></a>");
    client.println("</body>");
    client.println();
  } else {
    //Constrói mensagem de erro e botão de voltar.
    client.println("<h1>Sistema de manuten&ccedil;&atilde;o</h1>");
    client.println("<h2>Preencha os dados de operador!!</h2>");
    client.println("<a href='TelaInicial'><button>Voltar</button></a>");
  }
}
void TelaGravarTag(WiFiClient client) {
  /*Função TelaGravarTag(WiFiClient client)
    Objetivo:
      Construir a tela de gravação de manutenção na etiqueta.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  //Constrói a tela caso os dados do operador estejam preenchidos, caso contrário constrói uma tela de erro.
  if (DadosOperador == true) {
    //Constrói o cabeçalho e o css da página.
    Cabecalho(client);
    //Apresenta os dados do operador.
    DadosOperadorPreechido(client);
    //Solicita a aproximação da etiqueta e chama a função EscreverTag.
    client.println("<p>Aproxime o equipamento da etiqueta...</p>");
    EscreverTag();
    //A partir a construção da tela só vai ocorrer após a gravação da etiqueta.
    client.println("<p>Informa&ccedil;&atilde;o gravada na etiqueta!!!</p>");
    DadosRealizarManutencao = false;
    //Constrói botão para geração do comprovante.
    client.println("<a href='Comprovante'><button>Gerar comprovante</button></a>");
    TipoComprovante = 2;
    client.println("</body>");
  } else {
    //Constrói o cabeçalho e o css da página.
    Cabecalho(client);
    //Constrói mensagem de erro e botão de voltar.
    client.println("<h1>Sistema de manuten&ccedil;&atilde;o</h1>");
    client.println("<h2>Preencha os dados de operador!!</h2>");
    client.println("<a href='TelaInicial'><button>Voltar</button></a>");
  }
}
void Cabecalho(WiFiClient client) {
  /*Função Cabecalho(WiFiClient client)
    Objetivo:
      Construir o cabeçalho em html, contando a folha de estilos utilizadas em todas as telas.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("");
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head>");
  client.println("<meta charset=\"uft-8\" />");
  client.println("<meta name=\"viewport\" content=\"width=device-width\" />");
  client.println("<title>Tela Inicial</title>");
  client.println("<style>");
  client.println("body{");
  client.println("position: absolute;");
  client.println("top: 0px;");
  client.println("left: 0px;");
  client.println("width: 100%;");
  client.println("margin: 0;");
  client.println("padding: 0;");
  client.println("text-align: center;");
  client.println("}");
  client.println("h1{");
  client.println("background-color: black;");
  client.println("color: white;");
  client.println("margin:0px;");
  client.println("width: 100%;");
  client.println("padding: 2%;");
  client.println("font-size: 50px;");
  client.println("}");
  client.println("button{");
  client.println("width: 50%;");
  client.println("padding: 1%;");
  client.println("margin-top: 3%;");
  client.println("margin-bottom: 3%;");
  client.println("}");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<h1>Sistema de manuten&ccedil;&atilde;o</h1>");
}
void DadosOperadorPreechido(WiFiClient client) {
  /*Função DadosOperadorPreechido(WiFiClient client)
    Objetivo:
      Construir a apresentação dos dados do operador, utilizado na maioria das telas.
    Parâmetros: 
      WiFiClient client -> Dispositivo conectado, onde será construído a tela.
    Retorna: 
      Nada.
  */
  client.println("<p>Dados do Operador preenchidos:</p>");
  client.println("<p>Operador: " + NomeOperador + "</p>");
  client.println("<p>Matr&iacute;cula: " + MatriculaOperador + "</p>");
  client.println("<p></p>");
}
void LerTag() {
  /*Função LerTag()
    Objetivo:
      Realizar a comunicação com o modulo RC522 e realizar a leitura de todas as informações da etiqueta.
    Parâmetros:
    Retorna: 
      Nada.
  */
  //Limpa a variável global Tag antes da leitura
  Tag = "";
  //Repete o código para todos os blocos da memória interna da etiqueta.
  for (byte bloco = 1; bloco < 64; bloco++) {
    //Inicia a variável taglida como false
    boolean taglida = false;
    //Enquanto a etiqueta não for lida o código deve se repetir.
    while (taglida == false) {
      //Verifica se existe uma etiqueta presente e se é possível realizar a leitura.
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));
        //Prepara as variaveis para leitura.
        for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
        byte buffer_read[SIZE_BUFFER] = { 0 };
        byte tamanho = SIZE_BUFFER;
        boolean erro = false;
        //Tenta autenticar a etiqueta.
        status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloco, &key, &(mfrc522.uid));
        //Se a autenticação der erro, apresente na Serial, caso contrário realize a leitura.
        if (status != MFRC522::STATUS_OK) {
          Serial.print(F("Authentication failed: "));
          Serial.println(mfrc522.GetStatusCodeName(status));
        } else {
          //Enquanto não chegar no último bloco ou de erro, tente ler as informações da etiqueta
          while (erro == false && bloco < 64) {
            //Verifica se o bloco é um bloco de gravação e leitura.
            if ((bloco + 1) % 4 != 0) {
              //Realiza a leitura.
              status = mfrc522.MIFARE_Read(bloco, buffer_read, &tamanho);
              //Verifica se a leitura deu erro.
              if (status != MFRC522::STATUS_OK) {
                Serial.print(F("Reading failed: "));
                Serial.println(mfrc522.GetStatusCodeName(status));
                //Muda a variável erro para verdadeiro e volta um bloco.
                erro = true;
                bloco = bloco - 1;
              } else {
                Serial.print(F("\nDados bloco ["));
                Serial.print(bloco);
                Serial.print(F("]: "));
                //Grava os dados lidos na variável global valortag;
                for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++) {
                  valortag[i] = buffer_read[i];
                }
                //Grava o ID da etiqueta caso seja o primeiro bloco.
                if (bloco == 1) {
                  for (byte i = 0; i < mfrc522.uid.size; i++) {
                    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
                    Serial.print(mfrc522.uid.uidByte[i], HEX);
                    IDTag.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
                    IDTag.concat(String(mfrc522.uid.uidByte[i], HEX));
                  }
                }
                //Grava os dados lidos em String e grava na variável global Tag.
                String str = (char *)valortag;
                str = str.substring(0, 15);
                Serial.println(str);
                Tag += str;
                //Muda o valor da variável taglida para verdadeiro.
                taglida = true;
                //Avança para o próximo bloco.
                bloco++;
              }
            } else {
              //Caso não seja um bloco de leitura e gravação, pule o bloco.
              bloco++;
            }
          }
        }
        //Finalize a comunicação com o modulo RC522
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
    }
  }
  return;
}
void EscreverTag() {
  /*Função EscreverTag()
    Objetivo:
      Realizar a comunicação com o modulo RC522 e realizar a gravação de todas as informações da etiqueta.
    Parâmetros:
    Retorna: 
      Nada.
  */
  //Concatena todas as informações que devem ser gravadas na etiqueta adicionando um # entre elas.
  String EnvioTag = NomeOperador + "#" + MatriculaOperador + "#" + TipoOperacao + "#" + ItensSubstituidos + "#" + ItensAvariados + "#" + Horario;
  //Verifica se o tamanho dos dados gravados não ultrapassa o valor máximo permitido.
  if (EnvioTag.length() > 752) {
    Serial.println("os dados tem mais de 752 bytes");
    return;
  }
  /*Adiciona espaços em branco caso tenha mais informações gravadas na etiqueta que informações que vão ser gravadas.
  Caso seja verdade, as informações que vão ser gravadas recebem espaços em branco afim de deixar a etiqueta somente 
  com informações referentes a essa manutenção, sem pedaços de informações de outras manutenções.
  */
  while (EnvioTag.length() < Tag.length()) {
    EnvioTag += " ";
  }
  Serial.println(EnvioTag);
  //Repete o código para todos os blocos da memória interna da etiqueta.
  for (byte bloco = 1; bloco < 64; bloco++) {
    //Inicia a variável taglida como false
    boolean taglida = false;
    //Enquanto a etiqueta não for lida e ainda tiver informações a serem gravadas.
    while (taglida == false && EnvioTag.length() > 0) {
      //Verifica se existe uma etiqueta presente e se é possível realizar a leitura.
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));
        //Prepara as variáveis para leitura.
        for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
        //Tenta autenticar a etiqueta.
        status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloco, &key, &(mfrc522.uid));
        //Se a autenticação der erro, apresente na Serial, caso contrário realize a gravação.
        if (status != MFRC522::STATUS_OK) {
          Serial.print(F("PCD_Authenticate() failed: "));
          Serial.println(mfrc522.GetStatusCodeName(status));
        } else {
          boolean erro = false;
          String aux;
          //Enquanto não chegar no último bloco ou de erro, tente gravar as informações da etiqueta
          while (erro == false && bloco < 64) {
            //Verifica se o bloco é um bloco de gravação e leitura.
            if ((bloco + 1) % 4 != 0) {
              //Caso as informações restantes para gravação tenham menos de 16 caracteres.
              if (EnvioTag.length() < 16) {
                //Transforme a String para vetor de caracteres.
                EnvioTag.toCharArray((char *)buffer, MAX_SIZE_BLOCK);
                //Preencha o vetor com espaços até ter 16 caracteres.
                for (byte i = EnvioTag.length(); i < MAX_SIZE_BLOCK; i++) {
                  buffer[i] = ' ';
                  taglida = true;
                  erro = true;
                }
              } else {
                //Grave os próximos 16 caracteres das informações para gravação em uma String auxiliar.
                aux = EnvioTag.substring(0, MAX_SIZE_BLOCK);
                //Transforme a String para vetor de caracteres.
                aux.toCharArray((char *)buffer, MAX_SIZE_BLOCK);
              }
              //Realize a gravação das informações.
              status = mfrc522.MIFARE_Write(bloco, buffer, MAX_SIZE_BLOCK);
              //Verifique se gravação ocorreu corretamente.
              if (status != MFRC522::STATUS_OK) {
                Serial.print(F("MIFARE_Write() failed: "));
                Serial.println(mfrc522.GetStatusCodeName(status));
                //Em caso de erro, mude o valor da variável erro para verdadeiro e volte um bloco.
                bloco = bloco - 1;
                erro = true;
              } else {
                Serial.println(F("MIFARE_Write() success: "));
                if (EnvioTag.length() < 16) {
                  EnvioTag = "";
                } else {
                  EnvioTag = EnvioTag.substring(MAX_SIZE_BLOCK - 1);
                }
                //Avance um bloco e mude o valor da variável taglida para verdadeiro.
                bloco++;
                taglida = true;
              }
            } else {
              //Avance um bloco.
              bloco++;
            }
          }
        }
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
    }
  }
}
void LimpaVariaveis() {
  /*Função LimpaVariaveis()
    Objetivo:
      Limpa as variáveis que são utilizadas durante uma operação, 
      afim de não interferir na próxima operação realizada pelo usuário.
    Parâmetros:
    Retorna: 
      Nada.
  */
  TipoOperacao = "";
  ItensSubstituidos = "";
  ItensAvariados = "";
  Horario = "";
  OBS = "";
  Tag = "";
  IDTag = "";
  DadosRealizarManutencao = false;
}
String DecodeURL(String str) {
  /*Função DecodeURL(String str)
    Objetivo:
      Decodifica um texto em padrão url.
    Parâmetros:
      String str -> Texto a ser decodificado.
    Retorna: 
      String -> Texto decodificado.
  */
  //Cria uma variável que irá receber o texto decodificado.
  String encodedString = "";
  char c;
  char code0;
  char code1;
  //Repita o código para cada caractere do texto codificado.
  for (int i = 0; i < str.length(); i++) {
    //Grava o caractere atual na variável c
    c = str.charAt(i);
    //Troca + por espaço
    if (c == '+') {
      encodedString += ' ';
    } else if (c == '%') { 
      //Caso seja um caractere codificado, pegue os dois próximos caracteres e chame a função h2int.
      i++;
      code0 = str.charAt(i);
      i++;
      code1 = str.charAt(i);
      //Concatena o retorno da função h2int para code0 e code1 na variável c.
      c = (h2int(code0) << 4) | h2int(code1);
      encodedString += c;
    } else {
      encodedString += c;
    }
  }
  //Retorne o texto decodificado.
  return encodedString;
}
unsigned char h2int(char c) {
  /*Função h2int(char c)
    Objetivo:
      Transformar valores em hexadecimal para decimal.
    Parâmetros:
      char c -> Valor que vai ser transformado.
    Retorna: 
      unsingned char -> Valor transformado.
  */
  //Se o valor hexadecimal estiver entre 1 e 9, retorne ele.
  if (c >= '0' && c <= '9') {
    return ((unsigned char)c - '0');
  }
  //Se o valor hexadecimal estiver entre a e f, retorne ele subtraindo 'a' e somando 10.
  if (c >= 'a' && c <= 'f') {
    return ((unsigned char)c - 'a' + 10);
  }
  //Se o valor hexadecimal estiver entre A e F, retorne ele subtraindo 'a' e somando 10.
  if (c >= 'A' && c <= 'F') {
    return ((unsigned char)c - 'A' + 10);
  }
  return (0);
}
