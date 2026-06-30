#include "Modelitos.h"

//Lum:

float LuxToLumens(float Lux, float AreaM2) {return Lux * AreaM2;}

float LuxToWattPorM2(float Lux, TiposLuz Tipo)
{
	float Multiplicador;
	switch (Tipo)
	{
		case Solar: Multiplicador = 0.0079f; break;

		case Incandescente: Multiplicador = 0.0013f; break;

		case Fluororescente: Multiplicador = 0.004f; break;

		default: Multiplicador = 1.0f; break;
	}
    return Lux * Multiplicador;
}

//Umi:

float UmidadeMaxPorTemp(float Tcelsius) {return 6.112f * expf((17.67f * Tcelsius) / (Tcelsius + 243.5f));}

float PontOrvalho(float Tcelsius, float UR)
{
    float gama = logf(UR / 100.0f) + (17.67f * Tcelsius) / (243.5f + Tcelsius);
    return (243.5f * gama) / (17.67f - gama);
}

float UmidadeAbsoluta(float UR, float Tcelsius)
{
    float T_k = CelsiusToKelvin(Tcelsius);

    float T_orvalho = PontOrvalho(Tcelsius, UR);

    float P_vapor = UmidadeMaxPorTemp(T_orvalho);

    return (P_vapor * 100.0f) / (461.5f * T_k) * 1000.0f;
}

//Temp:

float CelsiusToKelvin(float Tcelsius) { return Tcelsius + 273.15f; }

float CelsiusToFahrenheit(float Tcelsius) { return Tcelsius * 1.8f + 32.0f; }

float SensacaoTermica(float Tcelsius, float UR)
{
    if (Tcelsius < 27.0f || UR < 40.0f) return Tcelsius;

    float Tf = CelsiusToFahrenheit(Tcelsius);
    float HI = -42.379f + 2.04901523f*Tf + 10.14333127f*UR
             - 0.22475541f*Tf*UR - 0.00683783f*Tf*Tf
             - 0.05481717f*UR*UR + 0.00122874f*Tf*Tf*UR
             + 0.00085282f*Tf*UR*UR - 0.00000199f*Tf*Tf*UR*UR;

    return (HI - 32.0f) / 1.8f;
}

//Alertas:

const char* AvaliarConfortoLuminoso(float Lux, TipoAmbiente Tipo)
{
    float limite_min, limite_max;

    switch (Tipo)
    {
        case AmbienteDescanso:
            limite_min = 100.0f; limite_max = 200.0f;
            break;
        case AmbienteTrabalho:
            limite_min = 300.0f; limite_max = 500.0f;
            break;
        case AmbientePrecisao:
            limite_min = 750.0f; limite_max = 1200.0f;
            break;
        default:
            limite_min = 300.0f; limite_max = 500.0f;
            break;
    }

    if (Lux < limite_min) return "Pouco";
    if (Lux > limite_max) return "Muito";
    return "Otimo";
}

const char* VerificarAlertaGelo(float Tcelsius, float UR)
{
    float T_orvalho = PontOrvalho(Tcelsius, UR);

    if (Tcelsius <= 4.0f && T_orvalho <= 0.0f)
    {
        return "Gelo";
    }

    if (UR >= 85.0f && (Tcelsius - T_orvalho) < 3.0f)
    {
        return "Condens";
    }

    return "Seguro";
}

const char* InteracaoLuzTemperatura(float Tcelsius)
{
    float Compara = (Tcelsius - 25.0f);

    if (Compara != 0.0f)
    {
        if (Compara < 0.0f) return "Otima";
        else                return "Alerta";
    }
    else return "Analise";
}

//Filtro:

void FiltroInit(FiltroExp* f, float Peso)
{
    f->PesoFiltro = Peso;
    f->Inicializado = 0x00;
    f->ValorFiltrado = 0.0f;
    f->ValorAnterior = 0.0f;
}

void FiltroAplicar(FiltroExp* f, float NovaLeitura)
{
    if (f->Inicializado == 0x00) {f->ValorFiltrado = NovaLeitura; f->Inicializado = 0x01;}
    else
    {
    	f->ValorAnterior = (NovaLeitura - f->ValorFiltrado);
    	f->ValorFiltrado = (f->PesoFiltro * NovaLeitura + (1.0f - f->PesoFiltro) * f->ValorFiltrado);
    }
}

float GetValorFiltro(FiltroExp* f){return f->ValorFiltrado;}

float GetValorAntFiltro(FiltroExp* f){return f->ValorAnterior;}


