#ifndef INC_MODELITOS_H_
#define INC_MODELITOS_H_

#include <math.h>
#include <string.h>
#include <stdint.h>

// Estrutura para filtro
typedef struct
{
    float ValorFiltrado;
    float ValorAnterior;
    float PesoFiltro;
    uint8_t Inicializado;
} FiltroExp;

typedef enum
{
	Solar,
	Incandescente,
	Fluororescente
}TiposLuz;

typedef enum {
    AmbienteDescanso,
    AmbienteTrabalho,
    AmbientePrecisao
} TipoAmbiente;


//Lum:

float LuxToLumens(float Lux, float AreaM2);

float LuxToWattPorM2(float Lux, TiposLuz Tipo);

//Umi:

float UmidadeMaxPorTemp(float Tcelsius);

float PontOrvalho(float Tcelsius, float UR);

float UmidadeAbsoluta(float UR, float Tcelsius);

//Temp:

float CelsiusToKelvin(float Tcelsius);

float CelsiusToFahrenheit(float Tcelsius);

float SensacaoTermica(float Tcelsius, float UR);

//Alertas:

const char* AvaliarConfortoLuminoso(float Lux, TipoAmbiente Tipo);

const char* VerificarAlertaGelo(float Tcelsius, float UR);

const char* InteracaoLuzTemperatura(float Tsensor);

//Filtro:

void FiltroInit(FiltroExp* f, float Peso);

void FiltroAplicar(FiltroExp* f, float NovaLeitura);

float GetValorFiltro(FiltroExp* f);

float GetValorAntFiltro(FiltroExp* f);

#endif /* INC_MODELITOS_H_ */
