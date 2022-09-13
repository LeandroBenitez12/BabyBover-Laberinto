#include "Pulsador.h"

Pulsador::Pulsador(int p)
{
    pin = p;

    pinMode(pin, INPUT);
}

void Pulsador::setFlanco(bool f)
{
    flanco = f;
    estado_anterior = !flanco;
}

bool Pulsador::getIsPress()
{
    bool estado_actual = digitalRead(pin);
    bool estado = (estado_anterior != estado_actual) && estado_actual == flanco;
    estado_anterior = estado_actual;
    return estado;
}