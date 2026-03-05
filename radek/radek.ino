#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include "BitOps.h"
 
const uint8_t pocet_disp = 12;
const uint8_t DIN = 0; // PD0
const uint8_t CS = 1;  // PD1
const uint8_t CLK = 2; // PD2
 
volatile uint8_t hodiny = 23; // volatile protože se pracuje s proměnnou v main ale i mimo main (přerušení)
volatile uint8_t minuty = 59;
volatile uint8_t sekundy = 45;
volatile uint8_t dny = 30;
volatile uint8_t mesice = 12;
volatile uint16_t roky = 2025;
 
volatile uint8_t dny_tyden = 0; //0-6
 
volatile uint8_t sekundy_prepnuti = 0; 
 
const uint8_t dvojtecka[8] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00};
const uint8_t tecka[8] = {0x18, 0x18,0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
 
const uint8_t tyden[14][8] = {
  {0x06,0x06,0x06,0x06,0x3E,0x66,0x66,0x3E}, // P (PO)0
  {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66}, // U (UT)1
  {0x3C,0x66,0x66,0x30,0x18,0x0C,0x66,0x3C}, // S (ST)2
  {0x3C,0x66,0x06,0x06,0x06,0x06,0x66,0x3C}, // C (CT)3
  {0x06,0x06,0x06,0x06,0x3E,0x66,0x66,0x3E}, // P (PA)4
  {0x3C,0x66,0x66,0x30,0x18,0x0C,0x66,0x3C}, // S (SO)5
  {0x66,0x66,0x66,0x76,0x7E,0x7E,0x6E,0x66}, // N (NE)6
  {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3C}, // O (PO)7
  {0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E}, // T (UT)8
  {0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E}, // T (ST)9
  {0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E}, // T (CT)10
  {0x66,0x66,0x66,0x7E,0x66,0x66,0x3C,0x18}, // A (PA)11
  {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3C}, // O (SO)12
  {0x7E,0x06,0x06,0x3E,0x06,0x06,0x06,0x7E}, // E (NE)13
};
 
const uint8_t cislo[10][8] = {
    {0x3C,0x66,0x66,0x66,0x6E,0x76,0x66,0x3C}, // 0
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x1C,0x18}, // 1
    {0x7E,0x7E,0x0C,0x18,0x30,0x60,0x66,0x3C}, // 2
    {0x3C,0x66,0x60,0x60,0x38,0x60,0x66,0x3C}, // 3
    {0x30,0x30,0x30,0x7E,0x36,0x3C,0x38,0x30}, // 4
    {0x3C,0x66,0x66,0x60,0x60,0x3E,0x06,0x7E}, // 5
    {0x3C,0x66,0x66,0x66,0x3E,0x06,0x0C,0x38}, // 6
    {0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x60,0x7E}, // 7
    {0x3C,0x66,0x66,0x66,0x3C,0x66,0x66,0x3C}, // 8
    {0x1C,0x30,0x60,0x60,0x7C,0x66,0x66,0x3C}, // 9
};
 
//-------Setup-------
 
void Setup()
{
  DDRD = 0xFF;   // Nastavení PORTD na výstupní
  PORTD = 0x00;  // Nastavení log. 0 na celý PORTD
  PORTD = Nastavit(PORTC, CS); // Nastavení CS do log. 1 - CS aktivní v log. 0 -> neaktivní
}
 
void Inicializace()
{
  for (int i = 0; i < pocet_disp; i++)
  {
    SendAll(0x0F, 0x00, i); // test off
    SendAll(0x09, 0x00, i); // dekodér off
    SendAll(0x0B, 0x07, i); // 8 řádků
    SendAll(0x0C, 0x01, i); // zapnout
    SendAll(0x0A, 0x08, i); // jas
    for (uint8_t row = 1; row <= 8; row++)
      SendAll(row, 0x00, i);  // Vyčistit matici (Vypnout všechny LED)
  }
}
 
void SetupTimer()
{
  TCCR1B |= (1 << WGM12);              // Nastavit Timer1 na CTC (Clear Timer on Compare) (18.9.2 dokumentace)
  TCCR1B |= (1 << CS12) | (1 << CS10); // Nastavit prescaler (děličku) na 1024 - Timer1 přičte hodnotu každých 1024 taktů (17.2, 18.4 dokumentace)
  OCR1A = 15624;                       // Nastavit Output Compare Register 1 A na 15624 (16MHz / 1024 = 15625 (1s)) Timer1 počítá od 0 až do 15624 (18.7 druhý odstavec dokumentace)
  TIMSK |= (1 << OCIE1A);              // V Timer Interrupt Mask Registru (18.2.1) nastavíme Output Compare Interrupt Enable A bit do High - Povolí přerušení při shodném porovnání TCNT1 s OCR1A (18.7) (Za 1s se 15625x přičte hodnota v Timer1 a potom se vynuluje)
}
 
//-------Přerušení-------
 
ISR(TIMER1_COMPA_vect) // Funkce volaná pokud dojde k přerušení - TCNT1 == OCR1A -> TIMER1_COMPA_vect (14.1)
{
  sekundy++;
  sekundy_prepnuti++;
  KontrolaHodnot();
}
 
 //-------Zobrazování jednoho znaku-------
 
void ChipSelect(bool aktivni)
{
  if (aktivni == true) // CS aktivni v log. 0 -> musíme ho vynulovat
    PORTD = Vynulovat(PORTD, CS);
  else
    PORTD = Nastavit(PORTD, CS);
}
 
void Clock()
{
  PORTD = Nastavit(PORTD, CLK);
  _delay_us(1);
  PORTD = Vynulovat(PORTD, CLK);
}
 
void SendByte(uint8_t data)
{
  for (int i = 7; i >= 0; i--) // Zkontrolujeme byte po individualních bitech od největšího a hodnotu pošleme na DIN pin a po každé kontrole "blikneme" clock
  {
    if (JeNastaven(data, i) == true)
      PORTD = Nastavit(PORTD, DIN);
    else
      PORTD = Vynulovat(PORTD, DIN);
    Clock();
  }
}
 
void SendAll(uint8_t reg, uint8_t data, uint8_t cil)
{
  ChipSelect(true); // Zapneme CS (vynulujeme)
  for (int8_t i = pocet_disp - 1 ; i >= 0; i--) // Projedeme všechny displeje od největšího
  {
    if (i == cil) // Pokud jsme na displeji na který chceme zapsat znak tak pošleme registr (řádek) a data (jaké LEDky checeme zapnout)
    {
      SendByte(reg);
      SendByte(data);
    }
    else // Jinak zapíšeme 0x00 na registr a data -> No Op podle dokumentace
    {
      SendByte(0x00);
      SendByte(0x00);
    }
  }
  ChipSelect(false); // Nakonec se vypne CS (nastavíme do log. 1)
}
 
void ZobrazitZnak(uint8_t row, uint8_t data, uint8_t cil)
{
  SendAll(row + 1, data, cil); // Row + 1 protože registry řádků jsou 1 - 8
}
 
//-------Celé zobrazování-------
 
void Cas(uint8_t row)
{
  for (int cil = 0; cil < 2; cil++) //Nevykreslovat nic
      ZobrazitZnak(row, 0x00, cil);
    ZobrazitZnak(row, cislo[hodiny / 10][row], 2); // Vykreslit znaky podle displeje
    ZobrazitZnak(row, cislo[hodiny % 10][row], 3);
    ZobrazitZnak(row, dvojtecka[row], 4);
    ZobrazitZnak(row, cislo[minuty / 10][row], 5);
    ZobrazitZnak(row, cislo[minuty % 10][row], 6);
    ZobrazitZnak(row, dvojtecka[row], 7);
    ZobrazitZnak(row, cislo[sekundy / 10][row], 8);
    ZobrazitZnak(row, cislo[sekundy % 10][row], 9);
    for (int cil = 10; cil < 12; cil++) // Opět nevykreslovat nic
      ZobrazitZnak(row, 0x00, cil);
}
 
void Datum(uint8_t row)
{
  ZobrazitZnak(row, cislo[dny / 10][row], 0);
  ZobrazitZnak(row, cislo[dny % 10][row], 1);
  ZobrazitZnak(row, tecka[row], 2);
  ZobrazitZnak(row, cislo[mesice / 10][row], 3);
  ZobrazitZnak(row, cislo[mesice % 10][row], 4);
  ZobrazitZnak(row, tecka[row], 5);
  ZobrazitZnak(row, cislo[roky / 1000][row], 6);
  ZobrazitZnak(row, cislo[(roky / 100) % 10][row], 7);
  ZobrazitZnak(row, cislo[(roky / 10) % 10][row], 8);
  ZobrazitZnak(row, cislo[roky % 10][row], 9);
  ZobrazitZnak(row, tyden[dny_tyden][row], 10);
  ZobrazitZnak(row, tyden[dny_tyden + 7][row], 11);
}
 
void ZobrazCas()
{
  for (uint8_t row = 0; row < 8; row++) // Na všechny displeje nejdříve zapíšeme první řádek všech znaků podle toho na jaké pozici mají být, potom druhý řádek atd.
  {
    Cas(row);
  }
}
 
void ZobrazDatum()
{
  for (uint8_t row = 0; row < 8; row++) // Stejné jako v ZobrazCas()
  {
    Datum(row);
  }
}
 
void EfektCas()
{
  for (uint8_t row = 0; row < 8; row++)
  {
    Cas(row);
    _delay_ms(62.5); //62.5 * 8 = 500ms na efekt
  }
}
 
void EfektDatum()
{
  for (uint8_t row = 0; row < 8; row++)
  {
    Datum(row);
    _delay_ms(62.5);
  }
}
 
//-------Přetečení-------
 
void Preteceni8(uint8_t *hodnota, uint8_t limit, uint8_t *dalsi, bool nula, bool tyden)
{
  if (*hodnota >= limit) // *hodnota -> hodnota na adrese proměnné (např. sekundy) // *hodnota = &sekundy -> zapsání adresy proměnné sekundy do pointru hodnota
  {                      //  hodnota -> adresa proměnné zapsané v pointru          // &sekundy -> adresa k hodnotě v paměti
    if (nula == true) // Pokud se přetečení vrací do 0
    {
      *hodnota = 0;
      (*dalsi)++; // Přistupuje a mění hodnotu, ne adresu kvůli ()!
      if (tyden == true) // Pokud je potřeba přičíst i den v týdnu
        dny_tyden++;
    }
    else // Pokud se přetečení vrací do 1
    {
      *hodnota = 1;
      (*dalsi)++;
    }
  }
}
 
void Preteceni16(uint8_t *hodnota, uint8_t limit, uint16_t *dalsi) // Stejné jako Preteceni8 ale pro roky -> uint16_t
{
  if (*hodnota >= limit)
  {
    *hodnota = 1;
    (*dalsi)++;
  }
}
 
void KontrolaHodnot()
{
  Preteceni8(&sekundy, 60, &minuty, true, false); // Vložení adres na proměnné do funkce (&)
  Preteceni8(&minuty, 60, &hodiny, true, false);
  Preteceni8(&hodiny, 24, &dny, true, true);
  Preteceni8(&dny, 31, &mesice, false, false);
  Preteceni16(&mesice, 13, &roky);
  if (dny_tyden > 6) // 0 - 6 -> pokud větší než 6 (7+) vydělit a vzít zbytek, tím získáme číslo 0 - 6, které stále odpovídá dnu v týdnu přesně
    dny_tyden = (dny_tyden % 7);
}
 
//-------Main-------
 
int main()
{
  Setup(); // Setup AVR PORTD
  Inicializace(); // Inicializace MAX7219
  SetupTimer(); // Nastavení a zapnutí Timeru1
  sei(); // Povolní globální přerušení - opak. funkce je cli() ta vypne globální přerušení
 
  while(1)
  {
    if (sekundy_prepnuti == 0) // Přechod z datumu na čas každých 20 vteřin
      EfektCas();
    else if (sekundy_prepnuti < 10) // Přepínaní každých 10 sekund
      ZobrazCas();
    else if (sekundy_prepnuti == 10) // Přechod z datumu na čas každých 20 vteřin
      EfektDatum();
    else if (sekundy_prepnuti < 20) // Přepínaní každých 10 sekund
      ZobrazDatum();
    else
      sekundy_prepnuti = 0; // Po 20 vteřinách nastavit sekundy_prepnuti na 0 -> opakovat cyklus
  }
  return 0;

}
