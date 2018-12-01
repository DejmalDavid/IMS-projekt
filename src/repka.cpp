////////////////////////////////////////////////////////////////////////////
// Lisovani repky                  SIMLIB/C++
//
// IMS projekt 2018 VUT FIT
//
// Autors : 
// David Dejmal xdejma00 
// Marek Kalabza xkalab09
//
//
//
// 1 time = 1 minuta a simuluje se pouze 12 h  6-18 hodin
// 1 repka je 10kg semene repky

#include "simlib.h"

#include <iostream>
#include <string.h>

#define DEN 720
#define MESIC 21600
#define HODINA 60

#define MAX_SILO 80000
unsigned int MAX_ZASOBNIK=10; 

Queue Silo("Silo");	//kapacita 800t 
Queue Zasobnik("Zasobnik u Lisu");	//kapacita dle druhu lisu  
Facility  Vysypka("Vysypka repky do sila");
Store Dopravnik("Dopravnik",10);
Facility Lis("Lis");

int day_counter=-1;
char vypis_cas[25];
int doba_prace_dopravnik=580;	//pracovni doba dopravniku = smena - doba prepravy - 10 minut cisteni na konci smeny
int doba_prace_lis=580;
bool perioda_flag=true;		//pomaha pri prijezdu aut pokud je kriticky stav sila
bool init_flag=true;
bool ventil_stop_flag=false;
double olej_dnes=0;
double odpad_dnes=0;
double olej_celkem=0;
double odpad_celkem=0;
Entity *Ventil;

char* sim_cas(double t,char* cas)
{
	int mesic,den,hodina,minuta;
	char help[10];
	mesic=(int)t/MESIC;
	t=t-(mesic*MESIC);
	den=(int)t/DEN;
	t=t-(den*DEN);
	hodina=(int)t/HODINA;
	t=t-(hodina*HODINA);
	//printf("%d.%d %.2d:%.2f\n",den,mesic,hodina,t);
	sprintf(help, "%d", den+1);
	strcpy(cas,help);
	sprintf(help, ".%d ", mesic+1);
	strcat(cas,help);
	sprintf(help, "%.2d", hodina+6);
	strcat(cas,help);
	sprintf(help, ":%.2f", t);
	strcat(cas,help);
	return cas;
}

class Repka : public Process {           
	void Behavior() {       
		//sim_cas(Time,vypis_cas);
		//printf("Repka start cas:%s\t\n",vypis_cas); //TODO
		if(init_flag==false)	//repka pri inicializaci nejde pres vysypku
		{
			Seize(Vysypka); 
			Wait(0.005);	//doba vysypani 10kg semene z kamionu
			Release(Vysypka); 
		}
		Into(Silo);
		//-------CEKAM V SILU--------------//
		Passivate();	//ceka na probuzeni od Ventil_silo
		//-------pusteni ventilem-------//
		Enter(Dopravnik);
		Wait(10);	//vysyp ze sila
		Leave(Dopravnik);
		Into(Zasobnik);
		sim_cas(Time,vypis_cas);
	//	printf("Repka zasobnik cas:%s\tzasobnik:%d\n",vypis_cas,Zasobnik.Length()); //TODO
		Passivate();
		//-------CEKAM V Zasobniku--------------//
		sim_cas(Time,vypis_cas);
		//printf("Repka lis cas:%s\t\n",vypis_cas); //TODO
		if(ventil_stop_flag==false)
		{
			Ventil->Activate();
		}
  	}
};

//Generator prijezdu Aut s repkou
class AutoGenerator : public Event {  
	void Behavior() {   	
	   	int mnozstvi_repky=Uniform(1000.0,1500.0);	//ruzne auta maji ruzny obsah
		init_flag=false;
		if(MAX_SILO>=(Silo.Length()+mnozstvi_repky))
		{
			for(int i=0;i<mnozstvi_repky;i++)
			{
				(new Repka)->Activate(); 
			}	
		}
		else
		{
			printf("PLNE SILO OJOJ");	//TODO
		}
		sim_cas(Time,vypis_cas);
		//printf("%g",Time);
		printf("Auto cas:%s\tobjem:%d\tSilo:%d\n",vypis_cas,mnozstvi_repky,Silo.Length()); //TODO
		if(Time<43200 && perioda_flag)		//auta prijizdeji pouze pres sklizen cca 2 mesice
		{
			Activate(Time+Exponential(720.0)); // exp(1den) TODO spatne
			perioda_flag=true;	//zabrani opetovnemu spousteni
		}
  	}
};

class Ventil_silo : public Process {           
	void Behavior() {     
		Entity *repka;
		while(Time<doba_prace_dopravnik+(day_counter*DEN))
		{
			ventil_stop_flag=false;	
			if(Silo.Length()<2000)	//kriticky stav sila
			{
				perioda_flag=false;
				(new AutoGenerator)->Activate();	//prijede extra kamion
				//TODO counter
				printf("Kriticke silo!");	
			}
			if(Zasobnik.Length()<MAX_ZASOBNIK)
			{	
				if((Zasobnik.Length()+Dopravnik.Used())<MAX_ZASOBNIK)
				{
					sim_cas(Time,vypis_cas);
					//printf("Ventil start cas:%s\tzasobnik:%d\n",vypis_cas,Zasobnik.Length());
					repka=Silo.GetFirst();
					repka->Activate();
					Wait(1);	//Vysypavani trva Otestovat! jestli me tady nevzbudi
					continue;
				}
			}
			Passivate();			
		}
  	}
};

class Lisovani : public Process {           
	void Behavior() {    
	   	double sichta;
		while(Time<doba_prace_lis+(day_counter*DEN))
		{
			sichta=(((day_counter)*DEN)+doba_prace_lis)-Time;	//zbyvajici cas smeny
	       		if(sichta/3.25<Zasobnik.Length()+Dopravnik.Used())			//blizi se konec smeny
			{
				//printf("sichta:%g\n",sichta/3.25);
				ventil_stop_flag=true;		//jiz nebude zapinat ventil
				//printf("ventil stop flag!\n");
			}		
			if(Zasobnik.Length()==0)	//konec smeny
			{	
				//printf("Prazdny zasobnik!\n");
				sim_cas(Time,vypis_cas);
				//printf("Konec smeny:%s\n",vypis_cas);
				break;
			}
			Seize(Lis);
			Entity *repka;
			repka=Zasobnik.GetFirst();
			repka->Activate();
			sim_cas(Time,vypis_cas);
			//printf("Lis mele cas:%s\tzasobnik:%d\n",vypis_cas,Zasobnik.Length());
			Wait(3.25);	//Trva lisovani
			olej_dnes=olej_dnes+1;
			odpad_dnes=odpad_dnes+9;
			Release(Lis); 		
		}
		//printf("Lis konec smeny\n");
  	}
};

class Day_Counter : public Event {  
	void Behavior() {
		sim_cas(Time,vypis_cas);
		printf("Smena %s - Silo:%d\tOlej:%g\tOdpad:%g\tZasobnik:%d\n",vypis_cas,Silo.Length(),olej_dnes,odpad_dnes,Zasobnik.Length());
		olej_celkem=olej_celkem+olej_dnes;
		odpad_celkem=odpad_celkem+odpad_dnes;
		olej_dnes=0;
		odpad_dnes=0;
		day_counter++;
		Ventil->Activate(Time+0.001);	//TODO vymyslet jak spustit tohle az po inicializaci sila
		(new Lisovani)->Activate(Time+11);
		Activate(Time+DEN);
	}
};



int main() {                   
	SetOutput("data.dat");	//vystupni data
	Print("Simulace lisovani repky\n");	
	Init(0,MESIC*12);		//Simulacni cas = 1 rok 26280
	for(int i=0;i<5000;i++)
	{
		(new Repka)->Activate(); 
	}
	Ventil=new Ventil_silo;
	(new AutoGenerator)->Activate(Time+Exponential(720.0));
 	(new Day_Counter)->Activate(Time);
	Run();   
	printf("\nCelkem Silo:%d\tOlej:%g\tOdpad:%g\n",Silo.Length(),olej_celkem,odpad_celkem);
	Silo.Output();
	Vysypka.Output();
	Zasobnik.Output();	
}

