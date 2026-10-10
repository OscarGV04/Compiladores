/*
 * Hands-on 1: Analisis Lexico y Automatas (version C++)
 * =========================================================
 *
 * Pipeline:
 *   1. Lexer (AFD a nivel de caracter)              -> tokens
 *   2. Validador sintactico (AFD a nivel de token)   -> objeto instruccion
 *   3. Maquina de Moore (automata con salida)        -> microoperaciones
 *
 * NOTA: no se usa <optional> a proposito, para que compile con versiones
 * viejas de MinGW (gcc.exe) sin soporte completo de C++17. En su lugar,
 * "direccion == -1" significa "sin direccion" (equivalente a null).
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <cctype>

using namespace std;

// =============================================================================
// 1. LEXER: AFD a nivel de caracter
// =============================================================================
//
// Estados del automata lexico:
//   S0        -> estado inicial / entre tokens
//   S_LETRA   -> acumulando letras (para MOV, ADD, STO, END, AL, BL)
//   S_DIGITO  -> acumulando digitos (para NUMERO)
//
// El lexer recorre la cadena caracter por caracter con "maximal munch":
// mientras el caracter siga en la misma categoria (letra o digito),
// permanece en el mismo estado acumulando el lexema; al cambiar de
// categoria, cierra el token actual y reinicia en S0 (reprocesando el
// caracter que provoco el cambio).

struct Token {
    string tipo;    // MOV, ADD, STO, END, REGISTRO, COMA, NUMERO
    string lexema;

    string repr() const {
        return tipo + "(\"" + lexema + "\")";
    }
};

class ErrorLexico : public runtime_error {
public:
    explicit ErrorLexico(const string& msg) : runtime_error(msg) {}
};

enum EstadoLexico { S0, S_LETRA, S_DIGITO };

string categoria(char c) {
    if (isalpha(static_cast<unsigned char>(c))) return "LETRA";
    if (isdigit(static_cast<unsigned char>(c))) return "DIGITO";
    if (c == ',') return "COMA";
    if (isspace(static_cast<unsigned char>(c))) return "ESPACIO";
    return "OTRO";
}

Token clasificarPalabra(const string& palabra) {
    static const vector<string> palabrasClave = {"MOV", "ADD", "STO", "END"};
    static const vector<string> registrosValidos = {"AL", "BL"};

    for (size_t i = 0; i < palabrasClave.size(); ++i) {
        if (palabra == palabrasClave[i]) {
            return Token{palabra, palabra};
        }
    }
    for (size_t i = 0; i < registrosValidos.size(); ++i) {
        if (palabra == registrosValidos[i]) {
            return Token{"REGISTRO", palabra};
        }
    }
    throw ErrorLexico("identificador no reconocido: '" + palabra + "'");
}

vector<Token> tokenizar(const string& cadena) {
    EstadoLexico estado = S0;
    string buffer;
    vector<Token> tokens;
    size_t i = 0;
    size_t n = cadena.size();

    while (i < n) {
        char c = cadena[i];
        string cat = categoria(c);

        if (estado == S0) {
            if (cat == "ESPACIO") {
                ++i;
            } else if (cat == "LETRA") {
                estado = S_LETRA;
                buffer = string(1, c);
                ++i;
            } else if (cat == "DIGITO") {
                estado = S_DIGITO;
                buffer = string(1, c);
                ++i;
            } else if (cat == "COMA") {
                tokens.push_back(Token{"COMA", ","});
                ++i;
            } else {
                throw ErrorLexico(string("caracter no valido: '") + c + "'");
            }
        } else if (estado == S_LETRA) {
            if (cat == "LETRA") {
                buffer += c;
                ++i;
            } else {
                tokens.push_back(clasificarPalabra(buffer));
                buffer.clear();
                estado = S0;
            }
        } else if (estado == S_DIGITO) {
            if (cat == "DIGITO") {
                buffer += c;
                ++i;
            } else {
                tokens.push_back(Token{"NUMERO", buffer});
                buffer.clear();
                estado = S0;
            }
        }
    }

    // cerrar el ultimo lexema pendiente al llegar al fin de cadena
    if (estado == S_LETRA) {
        tokens.push_back(clasificarPalabra(buffer));
    } else if (estado == S_DIGITO) {
        tokens.push_back(Token{"NUMERO", buffer});
    }

    return tokens;
}

// =============================================================================
// 2. VALIDADOR SINTACTICO: AFD a nivel de token
// =============================================================================
//
// La tabla de transicion es un mapa (estado, tipo_de_token) -> estado
// siguiente. Los estados que empiezan con "ACEPTA_" son de aceptacion.

class ErrorSintactico : public runtime_error {
public:
    explicit ErrorSintactico(const string& msg) : runtime_error(msg) {}
};

struct Instruccion {
    string operacion;
    vector<string> registros;
    int direccion; // -1 significa "sin direccion" (equivalente a null)
};

// clave de la tabla: "estado|tipoToken"
map<string, string> construirTransiciones() {
    map<string, string> t;
    t["q0|MOV"] = "q_mov_1";
    t["q0|ADD"] = "q_add_1";
    t["q0|STO"] = "q_sto_1";
    t["q0|END"] = "ACEPTA_END";

    t["q_mov_1|REGISTRO"] = "q_mov_2";
    t["q_mov_2|COMA"] = "q_mov_3";
    t["q_mov_3|NUMERO"] = "ACEPTA_MOV";

    t["q_add_1|REGISTRO"] = "q_add_2";
    t["q_add_2|COMA"] = "q_add_3";
    t["q_add_3|REGISTRO"] = "ACEPTA_ADD";

    t["q_sto_1|NUMERO"] = "ACEPTA_STO";
    return t;
}

map<string, string> construirMensajesError() {
    map<string, string> m;
    m["q0"] = "mnemonico no reconocido o instruccion vacia";
    m["q_mov_1"] = "se esperaba un registro (AL o BL) despues de MOV";
    m["q_mov_2"] = "falta la coma entre el registro y la direccion";
    m["q_mov_3"] = "se esperaba una direccion numerica despues de la coma";
    m["q_add_1"] = "se esperaba el registro AL despues de ADD";
    m["q_add_2"] = "falta la coma entre AL y BL";
    m["q_add_3"] = "se esperaba el registro BL despues de la coma";
    m["q_sto_1"] = "se esperaba una direccion numerica despues de STO";
    return m;
}

bool esEstadoAceptacion(const string& estado) {
    return estado == "ACEPTA_MOV" || estado == "ACEPTA_ADD" ||
           estado == "ACEPTA_STO" || estado == "ACEPTA_END";
}

Instruccion validarYConstruir(const vector<Token>& tokens) {
    static map<string, string> transiciones = construirTransiciones();
    static map<string, string> mensajesError = construirMensajesError();

    if (tokens.empty()) {
        throw ErrorSintactico("instruccion vacia");
    }

    string estado = "q0";
    for (size_t idx = 0; idx < tokens.size(); ++idx) {
        const Token& tok = tokens[idx];

        // Validaciones semanticas puntuales dentro de la transicion de ADD
        if (estado == "q_add_1" && tok.tipo == "REGISTRO" && tok.lexema != "AL") {
            throw ErrorSintactico("en ADD, el primer registro debe ser AL");
        }
        if (estado == "q_add_3" && tok.tipo == "REGISTRO" && tok.lexema != "BL") {
            throw ErrorSintactico("en ADD, el segundo registro debe ser BL");
        }

        string clave = estado + "|" + tok.tipo;
        map<string, string>::iterator it = transiciones.find(clave);
        if (it == transiciones.end()) {
            string base = mensajesError.count(estado) ? mensajesError[estado]
                                                        : "secuencia de tokens invalida";
            throw ErrorSintactico(base + " (token inesperado: " + tok.repr() + ")");
        }
        estado = it->second;
    }

    if (!esEstadoAceptacion(estado)) {
        string base = mensajesError.count(estado) ? mensajesError[estado]
                                                    : "instruccion incompleta";
        throw ErrorSintactico(base);
    }

    // --- Construccion del objeto instruccion segun el estado de aceptacion ---
    Instruccion instr;
    instr.direccion = -1;

    if (estado == "ACEPTA_MOV") {
        instr.operacion = "MOV";
        instr.registros.push_back(tokens[1].lexema);
        instr.direccion = stoi(tokens[3].lexema);
    } else if (estado == "ACEPTA_ADD") {
        instr.operacion = "ADD";
        instr.registros.push_back("AL");
        instr.registros.push_back("BL");
    } else if (estado == "ACEPTA_STO") {
        instr.operacion = "STO";
        instr.direccion = stoi(tokens[1].lexema);
    } else if (estado == "ACEPTA_END") {
        instr.operacion = "END";
    }

    return instr;
}

// =============================================================================
// 3. MAQUINA DE MOORE: genera las microoperaciones
// =============================================================================
//
// Cada estado tiene una salida asociada (plantilla) y un estado siguiente.
// El objeto instruccion aporta los parametros que se sustituyen al emitir
// la salida. La maquina nunca vuelve a leer el texto original.

struct SalidaMoore {
    string plantilla;
    string siguiente;
};

map<string, SalidaMoore> construirTablaMoore() {
    map<string, SalidaMoore> t;
    t["MOV_PREPARAR"] = SalidaMoore{"MAR <- {direccion}", "MOV_LEER"};
    t["MOV_LEER"]     = SalidaMoore{"MBR <- M[MAR]",       "MOV_CARGAR"};
    t["MOV_CARGAR"]   = SalidaMoore{"{registro} <- MBR",   "FIN"};

    t["ADD_SUMAR"]    = SalidaMoore{"ACC <- AL + BL",      "FIN"};

    t["STO_PREPARAR"] = SalidaMoore{"MAR <- {direccion}",  "STO_MBR"};
    t["STO_MBR"]      = SalidaMoore{"MBR <- ACC",          "STO_MEMORIA"};
    t["STO_MEMORIA"]  = SalidaMoore{"M[MAR] <- MBR",       "FIN"};

    t["END_HALT"]     = SalidaMoore{"HALT <- 1",           "FIN"};
    return t;
}

string estadoInicialPorOperacion(const string& operacion) {
    if (operacion == "MOV") return "MOV_PREPARAR";
    if (operacion == "ADD") return "ADD_SUMAR";
    if (operacion == "STO") return "STO_PREPARAR";
    return "END_HALT"; // operacion == "END"
}

// sustituye {direccion} y {registro} en la plantilla por sus valores reales
string sustituirPlaceholders(const string& plantilla, const Instruccion& instr) {
    string resultado = plantilla;

    size_t pos = resultado.find("{direccion}");
    if (pos != string::npos) {
        resultado.replace(pos, string("{direccion}").size(), to_string(instr.direccion));
    }

    pos = resultado.find("{registro}");
    if (pos != string::npos) {
        string registro = instr.registros.empty() ? "" : instr.registros[0];
        resultado.replace(pos, string("{registro}").size(), registro);
    }

    return resultado;
}

class MaquinaMoore {
public:
    explicit MaquinaMoore(const Instruccion& instr)
        : instruccion(instr), estado(estadoInicialPorOperacion(instr.operacion)) {
        tabla = construirTablaMoore();
    }

    // Emite la salida del estado actual y avanza al siguiente estado.
    // Devuelve "" si ya se llego a FIN.
    string siguientePaso() {
        if (estado == "FIN") return "";
        SalidaMoore s = tabla[estado];
        string salida = sustituirPlaceholders(s.plantilla, instruccion);
        estado = s.siguiente;
        return salida;
    }

    vector<string> generarTodas() {
        vector<string> salidas;
        while (estado != "FIN") {
            salidas.push_back(siguientePaso());
        }
        return salidas;
    }

private:
    Instruccion instruccion;
    string estado;
    map<string, SalidaMoore> tabla;
};

// =============================================================================
// 4. PROGRAMA PRINCIPAL: integra las tres etapas y da el formato de salida
// =============================================================================

string listaARegistros(const vector<string>& registros) {
    string s = "[";
    for (size_t i = 0; i < registros.size(); ++i) {
        s += "\"" + registros[i] + "\"";
        if (i + 1 < registros.size()) s += ", ";
    }
    s += "]";
    return s;
}

void procesarInstruccion(const string& texto) {
    cout << "ENTRADA\n" << texto << "\n\n";

    // --- Etapa 1: AFD lexico ---
    vector<Token> tokens;
    try {
        tokens = tokenizar(texto);
    } catch (const ErrorLexico& e) {
        cout << "VALIDACION MEDIANTE AFD\n";
        cout << "Instruccion invalida: " << e.what() << "\n";
        cout << "No se construye el objeto instruccion.\n";
        cout << "No se generan microoperaciones.\n\n";
        cout << string(60, '-') << "\n\n";
        return;
    }

    // --- Etapa 2: AFD sintactico + construccion del objeto ---
    Instruccion instr;
    try {
        instr = validarYConstruir(tokens);
    } catch (const ErrorSintactico& e) {
        cout << "VALIDACION MEDIANTE AFD\n";
        cout << "Instruccion invalida: " << e.what() << ".\n";
        cout << "No se construye el objeto instruccion.\n";
        cout << "No se generan microoperaciones.\n\n";
        cout << string(60, '-') << "\n\n";
        return;
    }

    cout << "VALIDACION MEDIANTE AFD\n";
    cout << "Instruccion valida.\n\n";

    cout << "TOKENS RECONOCIDOS\n";
    for (size_t i = 0; i < tokens.size(); ++i) {
        cout << tokens[i].repr() << "\n";
    }
    cout << "\n";

    cout << "COMPONENTES IDENTIFICADOS\n";
    cout << "Mnemonico: " << instr.operacion << "\n";
    if (!instr.registros.empty()) {
        cout << "Registro(s): ";
        for (size_t i = 0; i < instr.registros.size(); ++i) {
            cout << instr.registros[i];
            if (i + 1 < instr.registros.size()) cout << ", ";
        }
        cout << "\n";
    }
    if (instr.direccion != -1) {
        cout << "Direccion de memoria: " << instr.direccion << "\n";
    }
    cout << "\n";

    cout << "OBJETO INSTRUCCION\n";
    cout << "{\n";
    cout << "  operacion: \"" << instr.operacion << "\",\n";
    cout << "  registros: " << listaARegistros(instr.registros) << ",\n";
    cout << "  direccion: " << (instr.direccion == -1 ? "null" : to_string(instr.direccion)) << "\n";
    cout << "}\n\n";

    // --- Etapa 3: Maquina de Moore ---
    cout << "MICROOPERACIONES GENERADAS POR MOORE\n";
    MaquinaMoore moore(instr);
    vector<string> salidas = moore.generarTodas();
    for (size_t i = 0; i < salidas.size(); ++i) {
        cout << (i + 1) << ". " << salidas[i] << "\n";
    }
    cout << "Generacion terminada.\n\n";
    cout << string(60, '-') << "\n\n";
}

int main() {

    cout << "=============================================\n";
    cout << "     ANALIZADOR LEXICO Y AUTOMATAS - ISA\n";
    cout << "=============================================\n\n";

    cout << "Instrucciones permitidas:\n";
    cout << "  MOV AL, direccion\n";
    cout << "  MOV BL, direccion\n";
    cout << "  ADD AL, BL\n";
    cout << "  STO direccion\n";
    cout << "  END\n\n";

    cout << "Escriba SALIR para terminar el programa.\n\n";

    string entrada;

    while (true) {

        cout << "Ingrese una instruccion: ";

        getline(cin, entrada);

        // Permitir terminar el programa
        if (entrada == "SALIR") {
            cout << "\nPrograma terminado.\n";
            break;
        }

        // Procesar la instruccion ingresada
        procesarInstruccion(entrada);
    }

    return 0;
}
