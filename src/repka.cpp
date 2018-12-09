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
// 1 time = 1 minuta a simuluje se pouze 12 h  6-18 hodin denne
// 1 repka je 10kg semene repky
// 1 l semena = 0,65 kg semena

#include "simlib.h"

#include <iostream>
#include <string.h>

#define DEN 720		//den v minutach /2 -> simulace bezi pouze 12h ze dne
#define MESIC 21600	//mesic v minutach /2
#define HODINA 60	//hodina v minutach


//Makra pro parametrizaci programu- ZDE PROVADET UPRAVY
#define MAX_SILO 80000				//kapacita sila v 10kg
#define INIT_LIS Uniform(3000.0,8000.0)		//pocatecni mnozstvi repky v lisu z minule urody
#define DELKA_SIMULACE DEN			//delka simulace v minutach/casove makro
#define MNOZSTVI_REPKY Uniform(800.0,1200.0)	//obsah nakladniho vozu
#define DELKA_SKLIZNE MESIC*2			//cas po ktery prijizdeji nakladni auta v minutach
#define PRIJEZD_AUTA Exponential(DEN)		//za jak dlouho prijede dalsi auto
#define CAS_VYSYPKY 0.005			//jak dlouho trva vysypat 10kg repky z kamionu do sila
#define CAS_DOPRAVNIK 10			//cas straveny na dopravniku
#define ALERT_SILO 2000				//mnozstvi repky kdy je nutne objednat extra kamion
#define VENTIL_CASOVAC 1			//cas po kterem muze ventil nalozit znova na dopravnik
#define CAS_LISOVANI Uniform(4.6,6)		//jak rychle pomele lis 10kg semene ; lis A - 3,25 az 3.8; lib B 4,6 az 6,0
#define MAX_LISOVANI 6				//max delka lisovani; max hodnota z CAS_LISOVANI
#define POMER_OLEJ Uniform(0.10,0.15)		//vynos oleje z 10kg
#define MAX_ZASOBNIK 4				//maximalni mnozstvi zasobniku pred lisem; lis A - 6; lis B - 4
#define DOBA_PRACE_LIS 580			//pracovni doba lisu=smena-10m start smeny()-doba jednoho mleti-20m cisteni na konci smeny
#define PRIKON_DOPRAVNIK 0.5			//prikon dopravniku na prepravu 10kg semene
#define PRIKON_LIS CAS_LISOVANI*0.09		//prikon lisu na 10kg semene;lis A - 0.18 ;lib B 0.09
#define JIMKA_OLEJ 4000				//kapacita jimky na olej
#define JIMKA_ODPAD 800				//kapacita jimky na odpad;lis A 1000kg ; lis B 800kg

//Konec moznych uprav

//SIMLIB promene
Queue Silo("Silo");				//kapacita 800t 
Queue Zasobnik("Zasobnik u Lisu");		//kapacita dle druhu lisu  
Facility  Vysypka("Vysypka repky do sila");	//vysypavani
Store Dopravnik("Dopravnik",5);			//dopravnik s kapacitou
Facility Lis("Lis");				//samotny lis

//Programove promene
int day_counter=-1;		//pocet dni v tydnu
char vypis_cas[25];		//pomocny retezec pro vypis casu
bool perioda_flag=true;		//pomaha pri prijezdu aut pokud je kriticky stav sila
bool init_flag=true;		//inicializace repky v silu
bool ventil_stop_flag=false;	//zastaveni dopravniku na konci smeny
double olej=0;			//mnozstvi vylisovaneho oleje
double odpad=0;			//mnozstvi vylisovaneho odpadu
double jimka_olej=0;		//mnozstvi oleje v jimce
double jimka_odpad=0;		//mnozstvi odpadu v jimce
double olej_dnes=0;		//mnozstvi dnes vylisovaneho oleje
double odpad_dnes=0;		//mnozstvi dnes vylisovaneho odpadu
double olej_celkem=0;		//mnozstvi celkove oleje
double odpad_celkem=0;		//mnozstvi clekove odpadu 
double prikon_dopravnik_celkem=0;	//mnozstvi spotrebovane energie celkem dopravnikem
double prikon_dopravnik_dnes=0;		//mnozstvi spotrebovane enerfie dnes dopravnikem
double prikon_lis_celkem=0;		//mnozstvi spotrebovane energie celkem lisem
double prikon_lis_dnes=0;		//mnozstvi spotrebovane enerfie dnes lisem
int auto_dnes=0;		//pocet prijetych nakladnich aut dnes
int auto_celkem=0;		//pocet aut celkem
int obsluha=0;			//pocet volani obsluhy pro vyprazneni jimek
int alert_silo=0;		//pocet akutne volanych aut s repkou
int cisterna=0;			//pocet cisteren co vyvezly jimku oleje
Entity *Ventil;			//pomocny pointer

char* sim_cas(double t,char* cas)	//pomocna funkce pro prevod Time na datum a cas
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
		//printf("Repka start cas:%s\t\n",vypis_cas); 
		if(init_flag==false)	//repka pri inicializaci nejde pres vysypku
		{
			Seize(Vysypka); 
			Wait(CAS_VYSYPKY);	//doba vysypani 10kg semene z kamionu
			Release(Vysypka); 
		}
		Into(Silo);
		//-------CEKAM V SILU--------------//
		Passivate();	//ceka na probuzeni od Ventil_silo
		//-------pusteni ventilem-------//
		Enter(Dopravnik);
		Wait(CAS_DOPRAVNIK);	//cesta po dopravniku
		prikon_dopravnik_dnes=prikon_dopravnik_dnes+PRIKON_DOPRAVNIK;
		Leave(Dopravnik);
		Into(Zasobnik);
		sim_cas(Time,vypis_cas);
	//	printf("Repka zasobnik cas:%s\tzasobnik:%d\n",vypis_cas,Zasobnik.Length()); 
		Passivate();
		//-------CEKAM V Zasobniku--------------//
		sim_cas(Time,vypis_cas);
		//printf("Repka lis cas:%s\t\n",vypis_cas); 
		if(ventil_stop_flag==false)
		{
			Ventil->Activate();
		}
  	}
};

//Generator prijezdu Aut s repkou
class AutoGenerator : public Event {  
	void Behavior() {   	
	   	int mnozstvi_repky=MNOZSTVI_REPKY;	//ruzne auta maji ruzny obsah
		init_flag=false;
		if(MAX_SILO>=(Silo.Length()+mnozstvi_repky))
		{
			for(int i=0;i<mnozstvi_repky;i++)
			{
				(new Repka)->Activate(); 
			}
			auto_dnes++;	
		}
		else	//v realnem svete by nenastalo pripadne by se osetrilo
		{
			printf("PLNE SILO - neplatna simulace - upravte parametry!!!\n");
			exit(42);
		}
		//sim_cas(Time,vypis_cas);
		//printf("Auto cas:%s\tobjem:%d\tSilo:%d\n",vypis_cas,mnozstvi_repky,Silo.Length()); 
		if(Time<DELKA_SKLIZNE && perioda_flag)		//auta prijizdeji pouze pres sklizen cca 2 mesice
		{
			Activate(Time+PRIJEZD_AUTA); 
			perioda_flag=true;	//zabrani opetovnemu spousteni
		}
  	}
};

class Ventil_silo : public Process {           
	void Behavior() {     
		Entity *repka;
		while(true)	//pracovni doba, z teto smycky by ale nikdy nemel vyjit
		{
			ventil_stop_flag=false;	
			if(Silo.Length()<ALERT_SILO)	//kriticky stav sila
			{
				perioda_flag=false;
				(new AutoGenerator)->Activate();	//prijede extra kamion
				alert_silo++;
				//printf("Kriticke silo!");	
			}
			if(Zasobnik.Length()<MAX_ZASOBNIK)
			{	
				if((Zasobnik.Length()+Dopravnik.Used())<MAX_ZASOBNIK)
				{
					sim_cas(Time,vypis_cas);
					//printf("Ventil start cas:%s\tzasobnik:%d\n",vypis_cas,Zasobnik.Length());
					repka=Silo.GetFirst();
					repka->Activate();
					Wait(VENTIL_CASOVAC);	//Vysypavani trva Otestovat! jestli me tady nevzbudi
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
		while(Time<DOBA_PRACE_LIS+(day_counter*DEN))
		{
			sichta=(((day_counter)*DEN)+DOBA_PRACE_LIS)-Time;	//zbyvajici cas smeny
	       		if(sichta/MAX_LISOVANI<Zasobnik.Length()+Dopravnik.Used())			//blizi se konec smeny
			{
			//	printf("sichta:%.3f\n",sichta/3.25);
				ventil_stop_flag=true;		//jiz nebude zapinat ventil
			//	printf("ventil stop flag!\n");
			}		
			if(Zasobnik.Length()==0)	//konec smeny
			{	
				//printf("Prazdny zasobnik!\n");
				sim_cas(Time,vypis_cas);
				//	printf("Konec smeny:%s\n",vypis_cas);
				break;
			}
			Seize(Lis);
			Entity *repka;
			repka=Zasobnik.GetFirst();
			repka->Activate();
			sim_cas(Time,vypis_cas);
			//printf("Lis mele cas:%s\tzasobnik:%d\n",vypis_cas,Zasobnik.Length());
			Wait(CAS_LISOVANI);	//Trva lisovani
			olej=POMER_OLEJ*10;
			odpad=(1-POMER_OLEJ)*10;
			olej_dnes=olej_dnes+olej;
			jimka_olej=jimka_olej+olej;
			jimka_odpad=jimka_odpad+odpad;
			if(jimka_olej>JIMKA_OLEJ*0.9)	//volani obsluhy o vyliti jimka je skoro plna
			{
				//printf("Vylij me\n");	// counter statistik
				cisterna++;
				jimka_olej=0;
			}
			if(jimka_odpad>JIMKA_ODPAD*0.8)	//volani obsluhy o vysypani
			{
				//printf("Vysyp me\n");	// counter statistik
				obsluha++;
				odpad_dnes=odpad_dnes+jimka_odpad;
				jimka_odpad=0;
			}
			prikon_lis_dnes=prikon_lis_dnes+PRIKON_LIS;
			Release(Lis); 		
		}
		//sim_cas(Time,vypis_cas);
		//printf("Konec smeny:%s\n",vypis_cas);
  	}
};

class Day_Counter : public Event {  
	int den_v_tydnu=0;
	int den_count=0;
	void Behavior() {
		sim_cas(Time,vypis_cas);
		odpad_dnes=odpad_dnes+jimka_odpad;
		if(den_count!=0)
		{
			Print("%d\t%.3f\t\t\t%.3f\t\t\t%.3f\t\t\t\t%.3f\t\t%d\t\t\t%d\t\t\t%d\n",den_count,olej_dnes,odpad_dnes/1000,prikon_dopravnik_dnes+prikon_lis_dnes,Silo.Length()/100.0,auto_dnes,obsluha,Zasobnik.Length()*10);
		}
		//prepocet promenych
		olej_celkem=olej_celkem+olej_dnes;
		odpad_celkem=odpad_celkem+odpad_dnes;
		prikon_dopravnik_celkem=prikon_dopravnik_celkem+prikon_dopravnik_dnes;
		prikon_lis_celkem=prikon_lis_celkem+prikon_lis_dnes;
		auto_celkem=auto_celkem+auto_dnes;
		obsluha=0;
		auto_dnes=0;
		prikon_dopravnik_dnes=0;
		prikon_lis_dnes=0;
		jimka_odpad=0;
		olej_dnes=0;
		odpad_dnes=0;
		day_counter++;
		den_v_tydnu++;
		den_count++;
		if(den_v_tydnu==6)	//sobota nic se nedeje
		{

		}
		else if(den_v_tydnu==7)	//nedele jde se do kostela
		{
			den_v_tydnu=0;
		}
		else 	//jinak se lisuje
		{
			Ventil->Activate(Time+0.0001);	//offset kvuli inicializaci sila
			(new Lisovani)->Activate(Time+11);	//lis se zapne az 11 minut po zacatku smeny
		}
		Activate(Time+DEN);
	}
};



int main() {                   
	SetOutput("data.dat");	//vystupni data
	Print("Simulace lisovani repky\n");
	Print("Denni statistika\n");	
	Print("Den\tVylisovany olej[l]\tOdpad z lisovani[t]\tSpotrebovana energie[kWh]\tObsah Sila[t]\tPocet prijetych aut\tPocet volani obsluhy\tObsah zasobniku pred lisem[kg]\n");
	RandomSeed(time(0));
	Init(0,DELKA_SIMULACE);		//Simulacni cas = 1 rok 26280
	for(int i=0;i<INIT_LIS;i++)	//zustatek v silu z predchoziho roku
	{
		(new Repka)->Activate(); 
	}
	Ventil=new Ventil_silo;
	(new AutoGenerator)->Activate(Time+PRIJEZD_AUTA);
 	(new Day_Counter)->Activate(Time);
	Run();   
	Print("Celkova statistika\n");
	Print("Olej:%.3f\tOdpad:%.3f\tEnergie:%.3f\tSilo:%.3f\tPocet Aut:%d\tPocet cisteren:%d\tPocet akutne volanych aut:%d\n",olej_celkem,odpad_celkem/1000,prikon_dopravnik_celkem+prikon_lis_celkem,Silo.Length()/100.0,auto_celkem,cisterna,alert_silo);
	Silo.Output();
	Dopravnik.Output();
	Zasobnik.Output();
	Lis.Output();	
}

