// ==============================================================
//                 ORBITER MODULE: ARTEMIS2
//                  Part of the ORBITER SDK
//          Copyright (C) 2002-2004 Martin Schweiger
//                   All rights reserved
//
// ARTEMIS2.cpp
// Control module for ARTEMIS2 vessel class
//

// ==============================================================
#define ORBITER_MODULE
#include "orbitersdk.h"

#include "ARTEMIS2.h"
#include "SMmesh.h"
#include "ARTEMISVCMESH.h"
#include <UltraMath.h>
//FOR DIALGUE
#include "DlgCtrl.h"

const DWORD COMM_SEND_SLPD = 2302;
const DWORD COMM_SEND_SLP1 = 2303;
const DWORD COMM_SEND_SLP2 = 2304;
const DWORD COMM_SEND_SLP3 = 2305;
const DWORD COMM_SEND_SLP4 = 2306;
const DWORD COMM_SEND_SLPF = 2307;

#include <stdio.h>
#include <fstream>
#define LOADBMP(id) (LoadBitmap (g_Param.hDLL, MAKEINTRESOURCE (id)))
BOOL CALLBACK ARTEMIS2_DlgProc(HWND, UINT, WPARAM, LPARAM);


VISHANDLE MainExternalMeshVisual = 0;
PARTICLESTREAMSPEC rcs_stream = {
	0, 0.1, 60, 30.0, 0.1, 0.03, 8, 0, PARTICLESTREAMSPEC::EMISSIVE,
	PARTICLESTREAMSPEC::LVL_LIN, 0.1, 1,
	PARTICLESTREAMSPEC::ATM_FLAT, 0.2, 0.1
};

PARTICLESTREAMSPEC main_stream = {
	0, 1.0, 60, 40.0, 0.1, 0.04, 12, 0, PARTICLESTREAMSPEC::EMISSIVE,
	PARTICLESTREAMSPEC::LVL_LIN, 0.0, 0.05,
	PARTICLESTREAMSPEC::ATM_FLAT, 0.1, 0.1
};

PARTICLESTREAMSPEC land_stream = {
	0, 3, 60, 2.0, 0.1, 0.7, 6, 10, PARTICLESTREAMSPEC::EMISSIVE,
	PARTICLESTREAMSPEC::LVL_PSQRT, 0.0, 0.05,
	PARTICLESTREAMSPEC::ATM_FLAT, 0.1, 0.1
};

PARTICLESTREAMSPEC srb_stream = {
	0, 4.0, 30, 40.0, 0.4, 8.0, 12, 3.0, PARTICLESTREAMSPEC::DIFFUSE,
	PARTICLESTREAMSPEC::LVL_PSQRT, 0.5, 1,
	PARTICLESTREAMSPEC::ATM_FLAT, 1, 0.1
};
void ARTEMIS2::clbkVisualCreated(VISHANDLE vis, int refcount)
{
	dhMesh0 = GetDevMesh(vis, 0); // mains ccapsule




}
void VLiftCoeff(double aoa, double M, double Re, double* cl, double* cm, double* cd)
{
	static const double step = RAD * 30.0;
	static const double istep = 1.0 / step;
	static const int nabsc = 13;
	//	Angle of attack					 -180	-150	-120	-90		-60		-30		0		30	 60		90		120		150	  180
	//	static const double CL[nabsc] = {	0,	-0.12,	-0.1,	  0,	  0,	  0,	0,		0,	  0,	 0,		0.1,	0.12,	0};
	//	static const double CM[nabsc] = {	0,	0.0002,	0.0004,	0.0004,	0.0003,	0.0002,	0, -0.0002,-0.0003,-0.0004,-0.0004,-0.0002,	0};
	static const double CL[nabsc] = { 0, -0.12, -0.1, 0, 0, 0, 0, 0, 0, 0, 0.1, 0.12, 0 };
	static const double CM[nabsc] = { 0, -0.0002, -0.0004, -0.0004, -0.0003, -0.0002, 0, 0.0002, 0.0003, 0.0004, 0.0004, 0.0002, 0 };
	// lift and moment coefficients from -180 to 180 in 30 degree steps.

	aoa += PI;
	int idx = max(0, min(11, (int)(aoa * istep)));
	double d = aoa * istep - idx;
	*cl = CL[idx] + (CL[idx + 1] - CL[idx]) * d;
	*cm = (CM[idx] + (CM[idx + 1] - CM[idx]) * d) / 10;
	*cd = 0.25 + oapiGetInducedDrag(*cl, 1.27, 0.3);
}

// 2. horizontal lift component (vertical stabiliser and body)

void HLiftCoeff(double beta, double M, double Re, double* cl, double* cm, double* cd)
{
	static const double step = RAD * 30;
	static const double istep = 1.0 / step;
	static const int nabsc = 13;
	static const double CL[nabsc] = { 0, -0.1, -0.1, 0, 0, 0, 0, 0, 0, 0, 0.1, 0.1, 0 };

	beta += PI;
	int idx = max(0, min(11, (int)(beta * istep)));
	double d = beta * istep - idx;
	*cl = CL[idx] + (CL[idx + 1] - CL[idx]) * d;
	*cm = 0.0;
	*cd = 0.25 + oapiGetInducedDrag(*cl, 1.27, 0.3);
}


// Constructor
ARTEMIS2::ARTEMIS2(OBJHANDLE hObj, int fmodel)
	: VESSEL4(hObj, fmodel)



{
	DefineAnimations(); 

	PANELDEPLOYED = 0;

	HATCH_status = HATCH_UP;
	HATCH_proc = 0.0;

	SPANELFOLD_status = HATCH_UP;
	SPANELFOLD_proc = 0.0;

	TABLE_status = HATCH_UP;
	TABLE_proc = 0.0;
	for (int i = 0; i < 3; i++) {
		beacon[i].shape = BEACONSHAPE_STAR;
		beacon[i].pos = beaconpos + i;
		beacon[i].col = beaconcol + i;
		beacon[i].size = 0.2;
		beacon[i].falloff = 0.3;
		beacon[i].period = 1;
		beacon[i].duration = 1;
		beacon[i].tofs = (6 - i) * 0.2;
		beacon[i].active = true;
		AddBeacon(beacon + i);
	}

	{
		int(i) = 3;
		beacon[i].shape = BEACONSHAPE_STAR;
		beacon[i].pos = beaconpos + i;
		beacon[i].col = beaconcol + i;
		beacon[i].size = 0.2;
		beacon[i].falloff = 0.3;
		beacon[i].period = 1;
		beacon[i].duration = .1;
		beacon[i].tofs = (6 - i) * 0.2;
		beacon[i].active = true;
		AddBeacon(beacon + i);
	}
	{int(i) = 4;
	beacon[i].shape = BEACONSHAPE_STAR;
	beacon[i].pos = beaconpos + i;
	beacon[i].col = beaconcol + i;
	beacon[i].size = 0.2;
	beacon[i].falloff = 0.3;
	beacon[i].period = 1;
	beacon[i].duration = 1;
	beacon[i].tofs = (6 - i) * 0.2;
	beacon[i].active = true;
	AddBeacon(beacon + i);
	}



	beaconpos[0].x = -2.477;
	beaconpos[1].x = 2.477;
	beaconpos[2].x = 0;
	beaconpos[3].x = 0;
	beaconpos[4].x = -.65;

	beaconpos[0].z = 1.49;
	beaconpos[1].z = 1.49;
	beaconpos[2].z = 1.49;
	beaconpos[3].z = 1.49;
	beaconpos[4].z = 2.222935;

	beaconpos[0].y = 0;
	beaconpos[1].y = 0;
	beaconpos[2].y = 2.477;
	beaconpos[3].y = -2.477;
	beaconpos[4].y = 2.125246;


	COLOUR4 col_a = { 0, 0, 0, 0 };
	COLOUR4 col_white = { 1, 1, 1, 0 };

	docking_lightfront = (SpotLight*)AddSpotLight(_V(-.65, 2.222935, 2.125246), _V(0, 0, 1), 150, 1e-3, 0, 1e-3, RAD * 25, RAD * 60, col_white, col_white, col_a);

	docking_lightfront->Activate(false);

	JOINT1_proc = .5;
	LIFT_SPEED = .1;
	mode = 0;
	CHAIRDEPLOY = 0;

	BLOWNFAIRING = 0;
	WING1ROTATE_proc = .5;

	WING4ROTATE_proc = .5;
	WING3ROTATE_proc = .5;
	WING2ROTATE_proc = .5;

	panelstow = 0;
	panel1stow = 0;
	
	solp_proc = 0;
	solp_status = 2;
	solp_rot_status = 0;
	solp_rot_proc = 0;

	ARM2_check = SPANEL_STOP;
	WINGROT1_check = SPANEL_STOP;
	WINGROT2_check = SPANEL_STOP;
	WINGROT3_check = SPANEL_STOP;
	WINGROT4_check = SPANEL_STOP;

	arm_tip[0] = PANEL1CAM_POS;
	arm_tip[1] = PANEL1CAM_POS + _V(0.0, 1.0, 0.0); // to calculate EE attachment direction (-Z coordinate of attachment point is negative, so subtract 1 here)
	arm_tip[2] = PANEL1CAM_POS + _V(0.0, 0.0, -1.0); // to calculate rot vector for attachment

	arm_tip1[0] = PANEL2CAM_POS;
	arm_tip1[1] = PANEL2CAM_POS + _V(0.0, 1.0, 0.0); // to calculate EE attachment direction (-Z coordinate of attachment point is negative, so subtract 1 here)
	arm_tip1[2] = PANEL2CAM_POS + _V(0.0, 0.0, 1.0); // to calculate rot vector for attachment

	arm_tip2[0] = PANEL3CAM_POS;
	arm_tip2[1] = PANEL3CAM_POS + _V(0.0, -1.0, 0.0); // to calculate EE attachment direction (-Z coordinate of attachment point is negative, so subtract 1 here)
	arm_tip2[2] = PANEL3CAM_POS + _V(0.0, 0.0, 1.0); // to calculate rot vector for attachment

	arm_tip3[0] = PANEL4CAM_POS;
	arm_tip3[1] = PANEL4CAM_POS + _V(0.0, -1.0, 0.0); // to calculate EE attachment direction (-Z coordinate of attachment point is negative, so subtract 1 here)
	arm_tip3[2] = PANEL4CAM_POS + _V(0.0, 0.0, 1.0); // to calculate rot vector for attachment

	ARM1_OPERATING_SPEED = .1;
	//mode = 0;
	BEACON = 0;
	ofs = _V(0, 0, 0);
	string[0] = 0;

	//MET
	MET = 0;
	centerROT_arm = false;
	timer_state[0] = 0;
	timer_state[1] = 0;
	timer_state[2] = 0;
	timer_state[3] = 0;
	timer_state[4] = 0;
	timer_state[5] = 0;
	timer_state[6] = 0;
	timer_state[7] = 0;
	timer_state[8] = 0;
	METmode = 1;
	GMTmode = 0;

	vccameracase = 0;
	CAM = 0;

	t_0 = 0;
	SetConfig0_Ready();
}


GDIParams g_Param;

static const DWORD ntdvtx_geardown = 3;
static TOUCHDOWNVTX tdvtx_geardown[ntdvtx_geardown] = {
	{ _V(0, -.89, 2), 26150800, 18409627, 1.6, 0.1 },
	{ _V(-2, -.89, -2), 26150800, 18409627, 3.0, 0.2 },
	{ _V(2, -.89, -2), 26150800, 18409627, 3.0, 0.2 }
};



//
void ARTEMIS2::clbkSetClassCaps(FILEHANDLE cfg)
//void ARTEMIS2::SetTransporter()
{

	SetSize(3);
	SetEmptyMass(500.0);
	SetCW(0.09, 0.09, 2, 1.4);
	SetWingAspect(0.7);
	SetWingEffectiveness(2.5);
	SetCrossSections(_V(710.54, 2159.34, 207.95));
	SetSurfaceFrictionCoeff(0.07, 0.3);
	SetRotDrag(_V(0.10, 0.13, 0.04));
	SetMaxWheelbrakeForce(5e5);
	SetPMI(_V(743.84, 872.38, 148.19));
	SetTrimScale(0.1);
	SetCameraOffset(_V(-0.41, 2, 2.241));
	SetDockParams(_V(0, 0, 4.168), _V(0, 0, 1), _V(0, 1, 0));//NOSE DOCK0
	SetTouchdownPoints(_V(0, .0001, 10), _V(-1.6, .0001, -10), _V(1.6, .0001, -10));;
	EnableTransponder(true);

	MAINCAP = oapiLoadMesh("ARTEMIS\\NEWARTEMIS");
	SetMeshVisibilityMode(AddMesh(MAINCAP), MESHVIS_VC | MESHVIS_ALWAYS);

	VCMESH = oapiLoadMesh("ARTEMIS\\NEWARTEMISVC");
	SetMeshVisibilityMode(AddMesh(VCMESH), MESHVIS_VC | MESHVIS_ALWAYS);//MESH 1

	SM_MESH = oapiLoadMesh("ARTEMIS\\NEWSM1");
	SetMeshVisibilityMode(AddMesh(SM_MESH), MESHVIS_VC | MESHVIS_ALWAYS);//MESH 2

	COVERMESH = oapiLoadMesh("ARTEMIS\\ARTEMISCOVER");// m
	SetMeshVisibilityMode(AddMesh(COVERMESH), MESHVIS_VC | MESHVIS_ALWAYS);//MESH 3

	CHAIRS4MESH = oapiLoadMesh("ARTEMIS\\ARTEMIS4CHAIRDEPLOY");// mesh4
	SetMeshVisibilityMode(AddMesh(CHAIRS4MESH), MESHVIS_EXTERNAL | MESHVIS_VC);

	ORANGECREWMESH = oapiLoadMesh("ARTEMIS\\ARTEMIS4CREWORANGE");// mesh5
	SetMeshVisibilityMode(AddMesh(ORANGECREWMESH), MESHVIS_EXTERNAL | MESHVIS_VC);

	CHAIRSSTOWED = oapiLoadMesh("ARTEMIS\\ARTEMIS4CHAIRSTOWED");// mesh6
	SetMeshVisibilityMode(AddMesh(CHAIRSSTOWED), MESHVIS_NEVER);


	BLUE2CREW = oapiLoadMesh("ARTEMIS\\ARTEMISCREWBLUE");// mesh7
	SetMeshVisibilityMode(AddMesh(BLUE2CREW), MESHVIS_NEVER);

	

	BURNTHEATSHIELD = oapiLoadMesh("ARTEMIS\\ARTEMISHEATSHIELD");
	SetMeshVisibilityMode(AddMesh(BURNTHEATSHIELD), MESHVIS_NEVER);//8

	DROGUECHUTEMESH = oapiLoadMesh("ARTEMIS\\artemisdroguechute");
	SetMeshVisibilityMode(AddMesh(DROGUECHUTEMESH), MESHVIS_NEVER);//9

	MAINCHUTE = oapiLoadMesh("ARTEMIS\\ARTEMISMAINCHUTE");
	SetMeshVisibilityMode(AddMesh(MAINCHUTE), MESHVIS_NEVER);//10

	BALLONMESH = oapiLoadMesh("Orion-MPCV\\orion-balloons");
	SetMeshVisibilityMode(AddMesh(BALLONMESH), MESHVIS_NEVER);//11

	LASMESH = oapiLoadMesh("ARTEMIS\\ARMETISLAS");// mesh12
	SetMeshVisibilityMode(AddMesh(LASMESH), MESHVIS_NEVER);

	FAIR1MESH = oapiLoadMesh("ARTEMIS\\ARTEMISFAIR_1");//13
	FAIR2MESH = oapiLoadMesh("ARTEMIS\\ARTEMISFAIR_2");//14
	FAIR3MESH = oapiLoadMesh("ARTEMIS\\ARTEMISFAIR_3");//15
	SetMeshVisibilityMode(AddMesh(FAIR1MESH), MESHVIS_NEVER);
	SetMeshVisibilityMode(AddMesh(FAIR2MESH), MESHVIS_NEVER);
	SetMeshVisibilityMode(AddMesh(FAIR3MESH), MESHVIS_NEVER);

	BLACKTILEMESH = oapiLoadMesh("ARTEMIS\\ARTEMISBURNTBLACK");// mesh16
	SetMeshVisibilityMode(AddMesh(BLACKTILEMESH), MESHVIS_NEVER);



	cdr1_mfd = { 1,GRP_LEFTMFDSCREEN };
	plt1_mfd = { 1,GRP_RIGHTMFDSCREEN1 };
	cntr1_mfd = { 1,GRP_CENTERMFDSCREEN };
	//	bs1 = oapiLoadTexture("DM2/Crewed_Dragon2_char.dds", true);
	BURNT_tex = oapiLoadTexture("ARTEMISCAPSULE/newartemisburnt.dds");
	BURNT_tex1 = oapiLoadTexture("ARTEMISCAPSULE/orionhatch3burnt.dds");
	//BURNT_tex2 = oapiLoadTexture("ARTEMISCAPSULE/talontopburnt1.dds");
	//oapiSetTexture(MAINCAP, 26, BURNT_tex);
	


	

}

void ARTEMIS2::CHANGESTATE(void)


{
	//ClearMeshes(TRUE);
	//LOAD MESHES
	//MESHVIS_EXTERNAL | MESHVIS_VC






	double dz;		// z-offset for various configs
	switch (mode) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:	 //Fcapsule config
	case 10:
	case 11: dz = 0;	break;
	case 12: dz = -2;	break;
	}
	switch (mode) {
	case 2:		// Orbit config  SM JETTISONED
		if (!t_0) 		SetMeshVisibilityMode(3, MESHVIS_EXTERNAL | MESHVIS_VC);  //cover
		if (GetAltitude() < 82000) APPLYSKIN(); //SetMeshVisibilityMode(11, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield
		SetMeshVisibilityMode(2, MESHVIS_NEVER);//sm

		ShiftCentreOfMass(ofs);
		break;
	case 3:		// Drogue config
		if (!t_0)	SetMeshVisibilityMode(12, MESHVIS_EXTERNAL | MESHVIS_VC);//drogue
		SetMeshVisibilityMode(11, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield
		ShiftCentreOfMass(ofs);
		break;
	case 4:		// not used
	case 5:		// Chute config
		SetMeshVisibilityMode(13, MESHVIS_EXTERNAL | MESHVIS_VC);//mainchute
		SetMeshVisibilityMode(11, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield

		SetMeshVisibilityMode(2, MESHVIS_NEVER);//sm

		ShiftCentreOfMass(ofs);
		//	SetMeshVisibilityMode(AddMesh(chuteMesh, &ofs), MESHVIS_ALWAYS);
		//	ShiftCentreOfMass(ofs);
		break;
	case 6:		// Landed config
		//	AddMesh(balloonMesh, &ofs);
		SetMeshVisibilityMode(14, MESHVIS_EXTERNAL | MESHVIS_VC);//baloon
		SetMeshVisibilityMode(11, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield
		SetMeshVisibilityMode(12, MESHVIS_NEVER);//drogue
		SetMeshVisibilityMode(13, MESHVIS_NEVER);//main
		SetMeshVisibilityMode(2, MESHVIS_NEVER);//sm
		SetMeshVisibilityMode(3, MESHVIS_NEVER);//cover
		//SetMeshVisibilityMode(19, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield
		//	SetMeshVisibilityMode(MMUMeshUINT, MESHVIS_NEVER);
		APPLYSKIN();
		break;
	case 7:
		//AddMesh(coverMesh, &ofs);
		SetMeshVisibilityMode(3, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(2, MESHVIS_EXTERNAL | MESHVIS_VC);

		//AddMesh(smMesh, &ofs);
		//	AddMesh(solar0Mesh, &ofs);
		break;
	case 10:		// Orbit config
		SetMeshVisibilityMode(3, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(2, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(15, MESHVIS_ALWAYS);
		if (BLOWNFAIRING == 0) {
			SetMeshVisibilityMode(16, MESHVIS_EXTERNAL | MESHVIS_VC);
			SetMeshVisibilityMode(17, MESHVIS_EXTERNAL | MESHVIS_VC);
			SetMeshVisibilityMode(18, MESHVIS_EXTERNAL | MESHVIS_VC);
		}
		break;
	case 11:		// Orbit config
		SetMeshVisibilityMode(3, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(2, MESHVIS_EXTERNAL | MESHVIS_VC);


		break;

	case 12:
		SetMeshVisibilityMode(15, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(2, MESHVIS_NEVER);
		break;
	}

}



void ARTEMIS2::clbkPreStep(double simt, double simdt, double mjd) {
	double h;
	static double h1 = 0.0;
	if (oapiCameraMode() == CAM_COCKPIT) {

		if (CAM == 1) {


			oapiDelAnnotation(hNote);
			char imstr[256];
			strcpy(imstr, "WING1");
			VECTOR3 col = _V(0.8, 0.7, 0.1);
			hNote = oapiCreateAnnotation(true, 1, col);
			oapiAnnotationSetPos(hNote, 0.468, 0.05, 0.66, 0.1);
			oapiAnnotationSetText(hNote, imstr);



		}
		if (CAM == 2) {


			oapiDelAnnotation(hNote);
			char imstr[256];
			strcpy(imstr, "WING2");
			VECTOR3 col = _V(0.8, 0.7, 0.1);
			hNote = oapiCreateAnnotation(true, 1, col);
			oapiAnnotationSetPos(hNote, 0.468, 0.05, 0.66, 0.1);
			oapiAnnotationSetText(hNote, imstr);




		}
		if (CAM == 3) {



			oapiDelAnnotation(hNote);
			char imstr[256];
			strcpy(imstr, "WING3");
			VECTOR3 col = _V(0.8, 0.7, 0.1);
			hNote = oapiCreateAnnotation(true, 1, col);
			oapiAnnotationSetPos(hNote, 0.468, 0.05, 0.66, 0.1);
			oapiAnnotationSetText(hNote, imstr);




		}
		if (CAM == 4) {


			oapiDelAnnotation(hNote);
			char imstr[256];
			strcpy(imstr, "WING4");
			VECTOR3 col = _V(0.8, 0.7, 0.1);
			hNote = oapiCreateAnnotation(true, 1, col);
			oapiAnnotationSetPos(hNote, 0.468, 0.05, 0.66, 0.1);
			oapiAnnotationSetText(hNote, imstr);




		}


		if ((CAM == 0) && (vccameracase == 4)) {
			oapiDelAnnotation(hNote);

		}
		if ((CAM == 0) && (vccameracase == 0)) {
			oapiDelAnnotation(hNote);

		}
		if ((CAM == 0) && (vccameracase == 1)) {
			oapiDelAnnotation(hNote);

		}
		if ((CAM == 0) && (vccameracase == 2)) {
			oapiDelAnnotation(hNote);

		}

	}
	if (oapiGetFocusObject() != GetHandle())    // focus vessel is not my vessel
	{
		oapiDelAnnotation(hNote);

	}

	if (mode > 1)
		h = GetAltitude(ALTMODE_GROUND);

	//	int t = (int) (simt - t_0);

	switch (mode) {
	case 0:
	case 1:
		return;

	case 2:
		if (!(int)h1)
			if (h < 34000) // I assume h is altitude
			{
				//oapiSetTexture(MainExternalMeshVisual, 26, BURNT_tex);
				APPLYSKIN();
			}

		if (h < 6200)	// Check if we shall pop the cover
			if (!t_0) {
				blowCover();
				t_0 = 1;
				CHANGESTATE();
			}

		if (h < 6000) {	// Check if we deploy the drogue
			t_0 = 0;
			SetConfig3_Drogue();
		}
		return;

	case 3:
		AddForce(_V(10000, 0, 0), _V(0, 0.05, 0.1));	// rotate while hanging on the chutes
		AddForce(_V(0, -10, 0), _V(0, 0, -100));			// trim foot down

		if (h < 1200 && !t_0) {	// Check if we shall deploy the chute
			blowDrogue();
			t_0 = 1;
			CHANGESTATE();
		}

		if (h < 1000) {
			t_0 = 0;
			SetConfig4_Chute();		// = 5
			AddForce(_V(10000, 0, 0), _V(0, 0.05, 0.1));	// rotate while hanging on the chutes
			AddForce(_V(0, -5, 0), _V(0, 0, -100));			// trim foot down,

			if (oapiGetTimeAcceleration() > 10)
				oapiSetTimeAcceleration(10);
		}
		return;

	case 4:
	case 5:
		if (oapiGetTimeAcceleration() > 10)
			oapiSetTimeAcceleration(10);

		AddForce(_V(10000, 0, 0), _V(0, 0.05, 0.1));	// rotate while hanging on the chutes
		AddForce(_V(0, -10, 0), _V(0, 0, -100));		// trim foot down,

		if (h < 1.5) { // Check if we are close to the ground		
			if (oapiGetTimeAcceleration() > 1.0)
				oapiSetTimeAcceleration(1.0);
			blowChute();
			SetConfig6_Land();
		}
		return;

	case 6:
		//	oapiSetTexture(dhMesh0, 26, BURNT_tex);
		//	oapiSetTexture(dhMesh0, 25, BURNT_tex1);//hatch
		//	oapiSetTexture(dhMesh0, 31, BURNT_tex2);//blackwindows

		APPLYSKIN();





		//case OAPI_KEY_F6: // set tex and kill reflection
	/*
			COLOUR4 not_reflective;
			not_reflective.r = 0;
			not_reflective.g = 0;
			not_reflective.b = 0;
			not_reflective.a = 0;

			COLOUR4 not_fresnel;
			not_fresnel.r = 0;
			not_fresnel.g = 0;
			not_fresnel.b = 0;
			not_fresnel.a = 0;

			gcMeshMaterial(dhMesh0, 32, MESHM_REFLECT, &not_reflective, true);
			gcMeshMaterial(dhMesh0, 32, MESHM_FRESNEL, &not_fresnel, true);


			*/


		return;

	case 10:
		if (GetAltitude() > 108000) {	// Check if blow the fairings
			if (fairing)
				blowFairing();
			SetConfig11_Launch1();
			CHANGESTATE();
		}
		break;
	case 11:
		if (GetAltitude() > 109000) {	// Check if blow the LAS
			blowLas();
			SetConfig0_Ready();
			CHANGESTATE();
		}
		break;
	case 12:
		//		IncThrusterGroupLevel(THGROUP_MAIN, -0.001);

		if (GetPropellantMass(ph_main) < 100)
			if ((h - h1) < 0)				// moving downward

				if (GetAltitude() < 2200) { 	// Check if we shall jett the LAS
					blowLas();
					//					blowCover();
					SetConfig2_Reentry();
				}
		h1 = h;
		break;

	}	// end switch
}

void ARTEMIS2::DoSolpRot()

{
	if (solp_rot_status == 0)solp_rot_status = 1;

	if (solp_rot_status == 1)
	{
		double Solpangle;
		VECTOR3 rsun;
		VECTOR3 rsuninv;
		VECTOR3 rsunloc;
		GetGlobalPos(rsun);
		rsuninv = _V(-rsun.x, -rsun.y, -rsun.z);
		Global2Local(rsuninv, rsunloc);
		Solpangle = (PI * ((solp_rot_proc * 2))); // current rotation angle of panels
		//double sunangle = atan2(-rsunloc.x, rsunloc.z); // current angle of sun on ZX plane relative to z axis
		double sunangle = atan2(rsunloc.z, rsunloc.x); // current angle of sun on ZX plane relative to X axis
		sunangle = sunangle + PI;
		if (sunangle < 0) sunangle = sunangle + (2 * PI); // sanity check - angle always positive
		if (sunangle < 0) sunangle = sunangle + (2 * PI); // sanity check - angle always positive

		double angle_diff = (sunangle - Solpangle); // angular difference between sun and panels

		int a = 1;
		if (angle_diff < PI && angle_diff > 0)a = 1;			// check which way the panels should rotate
		else if (angle_diff > PI && angle_diff > 0)a = -1;	// check which way the panels should rotate

		else if (angle_diff > -PI && angle_diff < 0)a = -1;	// check which way the panels should rotate
		else if (angle_diff < -PI && angle_diff < 0)a = 1;	// check which way the panels should rotate

		double max_rot = step_time * SOLP_SPEED * 2 * PI;	// check if angular difference
		if (angle_diff > 0 && angle_diff < max_rot) a = 0;			// is less than panel rotation
		else if (angle_diff < 0 && -angle_diff < max_rot) a = 0;	// during this frame

		double db = step_time * SOLP_SPEED;
		if (a == 1) solp_rot_proc = (solp_rot_proc + db);
		else if (a == -1) solp_rot_proc = (solp_rot_proc - db);
		else if (a == 0) solp_rot_proc = sunangle / (2 * PI); // set exactly perpendicular to sun

		if (solp_rot_proc > 1) solp_rot_proc = (solp_rot_proc - 1); // range always 0-1
		else if (solp_rot_proc < 0) solp_rot_proc = (solp_rot_proc + 1); // range always 0-1

		//sprintf(oapiDebugString(), "sunangle %2.2fsunangledf %2.2f rotf %2.2f status %d ", sunangle,angle_diff, solp_rot_proc, solp_rot_status);

		//SetAnimation(anim_SPANEL1ROTATE, solp_rot_proc); // set the animation state and we're done!
	}

	else if (solp_rot_status == 2)
	{
		//sprintf(oapiDebugString(), " rotf %2.2f status %d ",  solp_rot_proc, solp_rot_status);
		double db = step_time * SOLP_SPEED;

		if (solp_rot_proc > 0.5)
		{
			solp_rot_proc = solp_rot_proc + db;
			if (solp_rot_proc > 1)
			{
				solp_rot_proc = 0;
				solp_rot_status = 3;
			}
		}
		else
		{
			solp_rot_proc = solp_rot_proc - db;
			if (solp_rot_proc < 0)
			{
				solp_rot_proc = 0;
				solp_rot_status = 3;
			}
		}

		if (solp_rot_proc > 1) solp_rot_proc = (solp_rot_proc - 1); // range always 0-1
		else if (solp_rot_proc < 0) solp_rot_proc = (solp_rot_proc + 1); // range always 0-1
		SetAnimation(anim_SPANEL1ROTATE, solp_rot_proc);
		SetAnimation(anim_SPANEL2ROTATE, solp_rot_proc);
		SetAnimation(anim_SPANEL3ROTATE, solp_rot_proc);
		SetAnimation(anim_SPANEL4ROTATE, solp_rot_proc);
	}
	else if (solp_rot_status == 3) {
		SetAnimation(anim_SPANEL1ROTATE, 0.5);
		SetAnimation(anim_SPANEL2ROTATE, 0.5);
		SetAnimation(anim_SPANEL3ROTATE, 0.5);
		SetAnimation(anim_SPANEL4ROTATE, 0.5);
	}
}


void ARTEMIS2::clbkPostStep(double simt, double simdt, double mjd)
{
	//MET
	// 	   //MET
	//MET += simdt;
	double groundspeed1 = GetGroundspeed();

	if (groundspeed1 > 0)MET += simdt;

	oapiVCTriggerRedrawArea(-1, AID_METTIME);

	timenew = floor(MET);
	days = timenew / (24 * 3600);

	timenew = timenew % (24 * 3600);
	hours = timenew / 3600;


	timenew %= 3600;
	minutes = timenew / 60;

	timenew %= 60;
	seconds = timenew;

	//dhMesh0 = NULL;

	//oapiSetTexture(MAINCAP, 26, BURNT_tex);

	double fMJD = oapiGetSimMJD();

	//SiameseCat edit: calculate GMT; leap year calculation accurate from 1970 to 2097 (I think)
	//41317 = 1.1.1972
	double fSimGMT = (fmod(fMJD - 41317, 365)) * 86400.0; //MJD 40952 == Jan. 1, 1970, 00:00:00
	int Days = (int)(fMJD - 41317.0);
	int leap_days = Days / 1460 + 1;
	fSimGMT -= leap_days * 86400.0; //compensate for leap years


	fSimGMT = fSimGMT + simdt;
	sGMTMillis = (short)fmod(fSimGMT * 1000.0, 1000.0);

	lTime = (long)fSimGMT;
	sGMTSeconds = lTime % 60;
	lTime /= 60;
	sGMTMinutes = lTime % 60;
	lTime /= 60;
	sGMTHours = lTime % 24;
	lTime /= 24;
	sGMTDays = lTime % 400 + 1;
	//sprintf(oapiDebugString(), "MET M: %d  G: %d GMT: %0.4f ", METmode,  GMTmode, fSimGMT);
	if (METmode == 1)
	{

		timer_state[8] = days / 100;//days1
		timer_state[7] = (days / 10) % 10;//days2
		timer_state[6] = days % 10;//days3
		timer_state[5] = hours / 10;
		timer_state[4] = hours % 10;
		timer_state[3] = minutes / 10;
		timer_state[2] = minutes % 10;
		timer_state[1] = seconds / 10;
		timer_state[0] = seconds % 10;
	}
	else if (GMTmode == 1)
	{
		//sprintf(oapiDebugString(), "timerstate1: %d timerstate2: %d timerstate3: %d timerstate4: %d", timer_state[8], timer_state[7], timer_state[6], timer_state[5]);
		//fSimGMT = fSimGMT + simdt;
	//	sGMTMillis = (short)fmod(fSimGMT * 1000.0, 1000.0);

	//	lTime = (long)fSimGMT;
	//	sGMTSeconds = lTime % 60;
	//	lTime /= 60;
	//	sGMTMinutes = lTime % 60;
	//	lTime /= 60;
	//	sGMTHours = lTime % 24;
	//	lTime /= 24;
	//	sGMTDays = lTime % 400 + 1;

		timer_state[8] = sGMTDays / 100;
		timer_state[7] = (sGMTDays / 10) % 10;
		timer_state[6] = sGMTDays % 10;
		timer_state[5] = sGMTHours / 10;
		timer_state[4] = sGMTHours % 10;
		timer_state[3] = sGMTMinutes / 10;
		timer_state[2] = sGMTMinutes % 10;
		timer_state[1] = sGMTSeconds / 10;
		timer_state[0] = sGMTSeconds % 10;
	}
	//wing fold
	SetAnimation(anim_SPANELROT, 1);

	if (SPANELFOLD_proc >= 1) {
		if (JOINT1_proc > 1)JOINT1_proc = (1);
		else if (JOINT1_proc < 0)JOINT1_proc = (0);
		double db = simdt * LIFT_SPEED;
		if (ARM2_check == SPANEL_DOWN)JOINT1_proc = (JOINT1_proc + db);
		else if (ARM2_check == SPANEL_UP)JOINT1_proc = (JOINT1_proc - db);
		else if (ARM2_check == SPANEL_STOP)JOINT1_proc = JOINT1_proc;

		if (JOINT1_proc > 1)JOINT1_proc = (1);
		else if (JOINT1_proc < 0)JOINT1_proc = (0);

		//sprintf(oapiDebugString(), " J1 %2.2f",   JOINT1_proc);
		SetAnimation(anim_SPANEL, JOINT1_proc);
	}

	if (WING1ROTATE_proc > 1)WING1ROTATE_proc = (0);
	else if (WING1ROTATE_proc < 0)WING1ROTATE_proc = (1);
	//double db = simdt * LIFT_SPEED;
	if (WINGROT1_check == SPANEL_DOWN)WING1ROTATE_proc = (WING1ROTATE_proc + db);
	else if (WINGROT1_check == SPANEL_UP)WING1ROTATE_proc = (WING1ROTATE_proc - db);
	else if (WINGROT1_check == SPANEL_STOP)WING1ROTATE_proc = WING1ROTATE_proc;

	if (WING1ROTATE_proc > 1)WING1ROTATE_proc = (0);
	else if (WING1ROTATE_proc < 0)WING1ROTATE_proc = (1);


	//PANEL2
	if (WING2ROTATE_proc > 1)WING2ROTATE_proc = (0);
	else if (WING2ROTATE_proc < 0)WING2ROTATE_proc = (1);
	//double db = simdt * LIFT_SPEED;
	if (WINGROT2_check == SPANEL_DOWN)WING2ROTATE_proc = (WING2ROTATE_proc + db);
	else if (WINGROT2_check == SPANEL_UP)WING2ROTATE_proc = (WING2ROTATE_proc - db);
	else if (WINGROT2_check == SPANEL_STOP)WING2ROTATE_proc = WING2ROTATE_proc;

	if (WING2ROTATE_proc > 1)WING2ROTATE_proc = (0);
	else if (WING2ROTATE_proc < 0)WING2ROTATE_proc = (1);

	//PANEL2
	if (WING3ROTATE_proc > 1)WING3ROTATE_proc = (0);
	else if (WING3ROTATE_proc < 0)WING3ROTATE_proc = (1);
	//double db = simdt * LIFT_SPEED;
	if (WINGROT3_check == SPANEL_DOWN)WING3ROTATE_proc = (WING3ROTATE_proc + db);
	else if (WINGROT3_check == SPANEL_UP)WING3ROTATE_proc = (WING3ROTATE_proc - db);
	else if (WINGROT3_check == SPANEL_STOP)WING3ROTATE_proc = WING3ROTATE_proc;

	if (WING3ROTATE_proc > 1)WING3ROTATE_proc = (0);
	else if (WING3ROTATE_proc < 0)WING3ROTATE_proc = (1);

	//PANEL3
	//PANEL2
	if (WING4ROTATE_proc > 1)WING4ROTATE_proc = (0);
	else if (WING4ROTATE_proc < 0)WING4ROTATE_proc = (1);
	//double db = simdt * LIFT_SPEED;
	if (WINGROT4_check == SPANEL_DOWN)WING4ROTATE_proc = (WING4ROTATE_proc + db);
	else if (WINGROT4_check == SPANEL_UP)WING4ROTATE_proc = (WING4ROTATE_proc - db);
	else if (WINGROT4_check == SPANEL_STOP)WING4ROTATE_proc = WING4ROTATE_proc;

	if (WING4ROTATE_proc > 1)WING4ROTATE_proc = (0);
	else if (WING4ROTATE_proc < 0)WING4ROTATE_proc = (1);


	SetAnimation(anim_SPANEL1ROTATE, WING1ROTATE_proc);
	SetAnimation(anim_SPANEL2ROTATE, WING2ROTATE_proc);
	SetAnimation(anim_SPANEL3ROTATE, WING3ROTATE_proc);
	SetAnimation(anim_SPANEL4ROTATE, WING4ROTATE_proc);

	if (WING1ROTATE_proc == .5)solp_status = 0;
	if (WING2ROTATE_proc == .5)solp_status = 0;
	if (WING3ROTATE_proc == .5)solp_status = 0;
	if (WING4ROTATE_proc == .5)solp_status = 0;
	if (solp_status == 0)DoSolpRot();


	step_time = simdt;
	//HATCH
	if (HATCH_status >= HATCH_RAISING) {
		double da = simdt * LIFT_SPEED;
		if (HATCH_status == HATCH_RAISING) {
			if (HATCH_proc > 0.0) HATCH_proc = max(0.0, HATCH_proc - da);
			else                HATCH_status = HATCH_UP;
		}
		else {
			if (HATCH_proc < 1.0) HATCH_proc = min(1.0, HATCH_proc + da);
			else                HATCH_status = HATCH_DOWN;
		}


		SetAnimation(anim_HATCH, HATCH_proc);

	}

	//table
	// 

	if (TABLE_status >= HATCH_RAISING) {
		double da = simdt * LIFT_SPEED;
		if (TABLE_status == HATCH_RAISING) {
			if (TABLE_proc > 0.0) TABLE_proc = max(0.0, TABLE_proc - da);
			else                TABLE_status = HATCH_UP;
		}
		else {
			if (TABLE_proc < 1.0) TABLE_proc = min(1.0, TABLE_proc + da);
			else                TABLE_status = HATCH_DOWN;
		}


		SetAnimation(anim_TABLE, TABLE_proc);
		
	}

	if (SPANELFOLD_status >= HATCH_RAISING) {
		double da = simdt * LIFT_SPEED;
		if (SPANELFOLD_status == HATCH_RAISING) {
			if (SPANELFOLD_proc > 0.0) SPANELFOLD_proc = max(0.0, SPANELFOLD_proc - da);
			else                SPANELFOLD_status = HATCH_UP;
		
			


		}
		else {
			if (SPANELFOLD_proc < 1.0) SPANELFOLD_proc = min(1.0, SPANELFOLD_proc + da);
			else                SPANELFOLD_status = HATCH_DOWN;
			
		}


		SetAnimation(anim_SPANELFOLD, SPANELFOLD_proc);
		
	}
	//panel stow
	if (panelstow == 1) {

		ARM2_check = SPANEL_STOP;
		double t0 = oapiGetSimTime();
		double dt = t0 - center_arm_t;       // time step
		double da1 = simdt * LIFT_SPEED * dt;  // total rotation angle
		//sprintf(oapiDebugString(), "dt  %2.2f t0  %2.2f da %2.2f ct %2.2f J1 %2.2f", dt, t0, da1, center_arm_t, JOINT1_proc);
		//	if (da1 && (JOINT1_proc != 0.5)) {    // zero shoulder yaw
		//	\\	if (da1 >= fabs(JOINT1_proc - 0.5)) // finished
		//	\\		JOINT1_proc = 0.5, da1 -= fabs(JOINT1_proc - 0.5);
		//	\\	else
		//	\\		JOINT1_proc -= (JOINT1_proc > 0.5 ? da1 : -da1), da1 = 0;

		if (JOINT1_proc > 0.5) JOINT1_proc = max(0.5, JOINT1_proc - da1);
		if (JOINT1_proc < 0.5) JOINT1_proc = min(0.5, JOINT1_proc + da1);



		SetAnimation(anim_SPANEL, JOINT1_proc);

		//		center_arm_t = t0;
		if (JOINT1_proc == 0.5) {

			center_arm = false; // finished stowing
			panelstow = 0;

		}
	}
	//stow panels
	if (centerROT_arm) {


		double t0 = oapiGetSimTime();
		double dt = t0 - centerROT_arm_t;       // time step
		double da = simdt * LIFT_SPEED * dt;  // total rotation angle
		//sprintf(oapiDebugString(), "dt  %2.2f t0  %2.2f da %2.2f ct %2.2f J1 %2.2f", dt,t0,da, center_arm_t, JOINT1_proc);
		if (da && (WING1ROTATE_proc != 0.5)) {    // zero shoulder yaw
			if (da >= fabs(WING1ROTATE_proc - 0.5)) // finished
				WING1ROTATE_proc = 0.5, da -= fabs(WING1ROTATE_proc - 0.5);
			else
				WING1ROTATE_proc -= (WING1ROTATE_proc > 0.5 ? da : -da), da = 0;
		}

		if (da && (WING2ROTATE_proc != 0.5)) {    // zero shoulder yaw
			if (da >= fabs(WING2ROTATE_proc - 0.5)) // finished
				WING2ROTATE_proc = 0.5, da -= fabs(WING2ROTATE_proc - 0.5);
			else
				WING2ROTATE_proc -= (WING2ROTATE_proc > 0.5 ? da : -da), da = 0;
		}
		if (da && (WING3ROTATE_proc != 0.5)) {    // zero shoulder yaw
			if (da >= fabs(WING3ROTATE_proc - 0.5)) // finished
				WING3ROTATE_proc = 0.5, da -= fabs(WING3ROTATE_proc - 0.5);
			else
				WING3ROTATE_proc -= (WING3ROTATE_proc > 0.5 ? da : -da), da = 0;
		}
		if (da && (WING4ROTATE_proc != 0.5)) {    // zero shoulder yaw
			if (da >= fabs(WING4ROTATE_proc - 0.5)) // finished
				WING4ROTATE_proc = 0.5, da -= fabs(WING4ROTATE_proc - 0.5);
			else
				WING4ROTATE_proc -= (WING4ROTATE_proc > 0.5 ? da : -da), da = 0;
		}
		SetAnimation(anim_SPANEL1ROTATE, WING1ROTATE_proc);
		SetAnimation(anim_SPANEL2ROTATE, WING2ROTATE_proc);
		SetAnimation(anim_SPANEL3ROTATE, WING3ROTATE_proc);
		SetAnimation(anim_SPANEL4ROTATE, WING4ROTATE_proc);

		if (WING1ROTATE_proc == .5)solp_status = 0;
		if (WING2ROTATE_proc == .5)solp_status = 0;
		if (WING3ROTATE_proc == .5)solp_status = 0;
		if (WING4ROTATE_proc == .5)solp_status = 0;
		if (solp_status == 0)DoSolpRot();


		step_time = simdt;
		//		center_arm_t = t0;
		if (da) {

			centerROT_arm = false; // finished stowing
			panel1stow = 0;

		}
	}
	if (CAM == 0) {
		SetCameraOffset(_V (-0.41, 2, 2.241));
		//STS()->SetCameraDefaultDirection (arm_tip[1]-arm_tip[0], 0.0);
		SetCameraDefaultDirection(_V(0, 0, 1));
	}
	if (CAM == 1) {  //ee view
		// calculate rotation angle for EE cam
		VECTOR3 dir = arm_tip[1] - arm_tip[0];
		// if camera is pointing straight up or down, make it slightly offset from (0,1,0) vector
		if (Eq(dotp(dir, _V(0, -1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, -0.999999984769, 0.0);
		else if (Eq(dotp(dir, _V(0, 1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, 0.999999984769, 0.0);
		VECTOR3 orbiter_cam_rot = crossp(crossp(dir, _V(0, 1, 0)), dir);
		orbiter_cam_rot /= length(orbiter_cam_rot);
		if (orbiter_cam_rot.y < 0) orbiter_cam_rot = -orbiter_cam_rot;
		double angle = SignedAngle(orbiter_cam_rot, arm_tip[2] - arm_tip[0], dir);

		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f dir: %f %f %f dot_prod: %f Angle: %f %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, dir.x, dir.y, dir.z, dot_prod, angle, angle*DEG);
		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f cam dir: %f %f %f dir: %f %f %f Angle: %f %f length: %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, arm_tip[2].x - arm_tip[0].x, arm_tip[2].y - arm_tip[0].y, arm_tip[2].z - arm_tip[0].z, dir.x, dir.y, dir.z, angle, angle*DEG, length(dir));
		//sprintf_s(oapiDebugString(), 255, "dot_prod: %f Angle: %f %f", dot_prod, angle, angle*DEG);
		//sprintf(oapiDebugString(), "armtip3x  %2.2f armtip3y %2.2f armtip3z %2.2f", arm_tip[0].x, arm_tip[0].y, arm_tip[0].z);
		SetCameraOffset(arm_tip[0]);
		//STS()->SetCameraDefaultDirection (arm_tip[1]-arm_tip[0], 0.0);
		SetCameraDefaultDirection(dir, angle);
		//	oapiCameraSetCockpitDir(0.0, 0.0);
	}
	if (CAM == 2) {  //ee view
	// calculate rotation angle for EE cam
		VECTOR3 dir = arm_tip1[1] - arm_tip1[0];
		// if camera is pointing straight up or down, make it slightly offset from (0,1,0) vector
		if (Eq(dotp(dir, _V(0, -1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, -0.999999984769, 0.0);
		else if (Eq(dotp(dir, _V(0, 1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, 0.999999984769, 0.0);
		VECTOR3 orbiter_cam_rot = crossp(crossp(dir, _V(0, 1, 0)), dir);
		orbiter_cam_rot /= length(orbiter_cam_rot);
		if (orbiter_cam_rot.y < 0) orbiter_cam_rot = -orbiter_cam_rot;
		double angle = SignedAngle(orbiter_cam_rot, arm_tip1[2] - arm_tip1[0], dir);

		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f dir: %f %f %f dot_prod: %f Angle: %f %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, dir.x, dir.y, dir.z, dot_prod, angle, angle*DEG);
		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f cam dir: %f %f %f dir: %f %f %f Angle: %f %f length: %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, arm_tip[2].x - arm_tip[0].x, arm_tip[2].y - arm_tip[0].y, arm_tip[2].z - arm_tip[0].z, dir.x, dir.y, dir.z, angle, angle*DEG, length(dir));
		//sprintf_s(oapiDebugString(), 255, "dot_prod: %f Angle: %f %f", dot_prod, angle, angle*DEG);
		//sprintf(oapiDebugString(), "armtip3x  %2.2f armtip3y %2.2f armtip3z %2.2f", arm_tip1[0].x, arm_tip1[0].y, arm_tip1[0].z);
		SetCameraOffset(arm_tip1[0]);
		//STS()->SetCameraDefaultDirection (arm_tip[1]-arm_tip[0], 0.0);
		SetCameraDefaultDirection(dir, angle);
		//oapiCameraSetCockpitDir(0.0, 0.0);
	}
	if (CAM == 3) {  //ee view
	// calculate rotation angle for EE cam
		VECTOR3 dir = arm_tip2[1] - arm_tip2[0];
		// if camera is pointing straight up or down, make it slightly offset from (0,1,0) vector
		if (Eq(dotp(dir, _V(0, -1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, -0.999999984769, 0.0);
		else if (Eq(dotp(dir, _V(0, 1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, 0.999999984769, 0.0);
		VECTOR3 orbiter_cam_rot = crossp(crossp(dir, _V(0, 1, 0)), dir);
		orbiter_cam_rot /= length(orbiter_cam_rot);
		if (orbiter_cam_rot.y < 0) orbiter_cam_rot = -orbiter_cam_rot;
		double angle = SignedAngle(orbiter_cam_rot, arm_tip2[2] - arm_tip2[0], dir);

		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f dir: %f %f %f dot_prod: %f Angle: %f %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, dir.x, dir.y, dir.z, dot_prod, angle, angle*DEG);
		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f cam dir: %f %f %f dir: %f %f %f Angle: %f %f length: %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, arm_tip[2].x - arm_tip[0].x, arm_tip[2].y - arm_tip[0].y, arm_tip[2].z - arm_tip[0].z, dir.x, dir.y, dir.z, angle, angle*DEG, length(dir));
		//sprintf_s(oapiDebugString(), 255, "dot_prod: %f Angle: %f %f", dot_prod, angle, angle*DEG);
		//sprintf(oapiDebugString(), "armtip3x  %2.2f armtip3y %2.2f armtip3z %2.2f", arm_tip2[0].x, arm_tip2[0].y, arm_tip2[0].z);
		SetCameraOffset(arm_tip2[0]);
		//STS()->SetCameraDefaultDirection (arm_tip[1]-arm_tip[0], 0.0);
		SetCameraDefaultDirection(dir, angle);
		//oapiCameraSetCockpitDir(0.0, 0.0);
	}

	if (CAM == 4) {  //ee view
	// calculate rotation angle for EE cam
		VECTOR3 dir = arm_tip3[1] - arm_tip3[0];
		// if camera is pointing straight up or down, make it slightly offset from (0,1,0) vector
		if (Eq(dotp(dir, _V(0, -1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, -0.999999984769, 0.0);
		else if (Eq(dotp(dir, _V(0, 1, 0)), 1.0, 1e-4)) dir = _V(1.74532924314e-4, 0.999999984769, 0.0);
		VECTOR3 orbiter_cam_rot = crossp(crossp(dir, _V(0, 1, 0)), dir);
		orbiter_cam_rot /= length(orbiter_cam_rot);
		if (orbiter_cam_rot.y < 0) orbiter_cam_rot = -orbiter_cam_rot;
		double angle = SignedAngle(orbiter_cam_rot, arm_tip3[2] - arm_tip3[0], dir);

		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f dir: %f %f %f dot_prod: %f Angle: %f %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, dir.x, dir.y, dir.z, dot_prod, angle, angle*DEG);
		//sprintf_s(oapiDebugString(), 255, "Rot Vec: %f %f %f cam dir: %f %f %f dir: %f %f %f Angle: %f %f length: %f", orbiter_cam_rot.x, orbiter_cam_rot.y, orbiter_cam_rot.z, arm_tip[2].x - arm_tip[0].x, arm_tip[2].y - arm_tip[0].y, arm_tip[2].z - arm_tip[0].z, dir.x, dir.y, dir.z, angle, angle*DEG, length(dir));
		//sprintf_s(oapiDebugString(), 255, "dot_prod: %f Angle: %f %f", dot_prod, angle, angle*DEG);
		//sprintf(oapiDebugString(), "armtip3x  %2.2f armtip3y %2.2f armtip3z %2.2f", arm_tip3[0].x, arm_tip3[0].y, arm_tip3[0].z);
		SetCameraOffset(arm_tip3[0]);
		//STS()->SetCameraDefaultDirection (arm_tip[1]-arm_tip[0], 0.0);
		SetCameraDefaultDirection(dir, angle);
		//oapiCameraSetCockpitDir(0.0, 0.0);
	}
	if (CHAIRDEPLOY == 0) {
		//SetMeshVisibilityMode(CHAIRDEPLOY4MeshUINT, MESHVIS_NEVER);
		SetMeshVisibilityMode(6, MESHVIS_NEVER);
		SetMeshVisibilityMode(7, MESHVIS_NEVER);
		//SetMeshVisibilityMode(astro1MeshUINT, MESHVIS_NEVER);
		
		SetMeshVisibilityMode(4, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(5, MESHVIS_EXTERNAL | MESHVIS_VC);
		
	}
	if (CHAIRDEPLOY == 1) {
		SetMeshVisibilityMode(6, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(5, MESHVIS_NEVER);
		SetMeshVisibilityMode(7, MESHVIS_NEVER);
		SetMeshVisibilityMode(4, MESHVIS_NEVER);

	}

	if (CHAIRDEPLOY == 2) {
		
		SetMeshVisibilityMode(7, MESHVIS_EXTERNAL | MESHVIS_VC);
		SetMeshVisibilityMode(6, MESHVIS_NEVER);
		SetMeshVisibilityMode(5, MESHVIS_NEVER);
	}

	if (BEACON == 0) {
		beacon[0].active = true;
		beacon[1].active = true;
		beacon[2].active = true;
		beacon[3].active = true;
	}

	if (BEACON == 1) {
		beacon[0].active = false;
		beacon[1].active = false;
		beacon[2].active = false;
		beacon[3].active = false;
	}
	if (lightON == 1) {
		beacon[4].active = true;
		docking_lightfront->Activate(true);
	}
	if (lightON == 0) {
		beacon[4].active = false;
		docking_lightfront->Activate(false);
	}

}



DLLCLBK VESSEL* ovcInit(OBJHANDLE hvessel, int flightmodel)
{
	return new ARTEMIS2(hvessel, flightmodel);
}

DLLCLBK void ovcExit(VESSEL* vessel)
{
	if (vessel) delete (ARTEMIS2*)vessel;
}

void ARTEMIS2::DefineAnimations(void)

{
//HATCH
	anim_HATCH = CreateAnimation(0.0);
	static UINT HATCHGrp[10] = { 0,12,18,19,21,34,51,110,123,122 };
	static MGROUP_ROTATE HATCH1(0, HATCHGrp, 10, _V(1.550, -.492, 2.87), _V(.5, 0, -.8), (float)(150 * RAD));
	AddAnimationComponent(anim_HATCH, 0, 1, &HATCH1);
	//METKNOB
	anim_TIMERKNOB = CreateAnimation(0.0);
	static UINT KNOBGrp[1] = { 26 };
	static MGROUP_ROTATE KNOB1(1, KNOBGrp, 1, _V(0, 1.211, 2.751275), _V(0, 0, 1), (float)(-65 * RAD));
	AddAnimationComponent(anim_TIMERKNOB, 0, 1, &KNOB1);

	//TABLE

	anim_TABLE = CreateAnimation(0.0);
	static UINT TABLEGrp[5] = { 42,97,96,119,120 };
	static MGROUP_ROTATE TABLE(0, TABLEGrp, 5, _V(-1.2458, -.1539425, 1.878698), _V(1, 0, 0), (float)(90 * RAD));
	AddAnimationComponent(anim_TABLE, 0, 1, &TABLE);

	//SM SOLAR PANELS
	ANIMATIONCOMPONENT_HANDLE  parent1, parent2, parent3, parent4, parent1A, parent2A, parent3A, parent4A, parent1ROTATE, parent2ROTATE, parent3ROTATE, parent4ROTATE;
	anim_SPANELFOLD = CreateAnimation(0.0);
	anim_SPANEL1ROTATE = CreateAnimation(0.5);
	anim_SPANEL2ROTATE = CreateAnimation(0.5);
	anim_SPANEL3ROTATE = CreateAnimation(0.5);
	anim_SPANEL4ROTATE = CreateAnimation(0.5);
	anim_SPANEL = CreateAnimation(0.5);
	anim_SPANELROT = CreateAnimation(0.0);
	//WING1

	static UINT SPANELGrp[1] = { GRP_PANEL1PARENT };
	static MGROUP_ROTATE SPANEL1(2, SPANELGrp, 1, _V(1.442652, 1.145286, -0.38705), _V(0, 0, 1), (float)(-60 * RAD));
	parent1A = AddAnimationComponent(anim_SPANELROT, 0, 1, &SPANEL1);

	static UINT sail2[2] = { GRP_PANEL1BASE1,GRP_PANEL1BASE2 };//sa_s1
	SOLAR1 = new MGROUP_ROTATE(2, sail2, 2, _V(1.591, 1.341, -1.678), _V(1, 0, 0), (float)(120 * RAD)); // -2 .. +145
	parent1 = AddAnimationComponent(anim_SPANEL, 0, 1, SOLAR1, parent1A);


	static UINT sail1[1] = { GRP_PANEL1ROTATE };//sa_s1
	SOLAR2 = new MGROUP_ROTATE(2, sail1, 1, _V(1.708, 1.446, -1.679), _V(0, 1, 0), (float)(-360 * RAD)); // -2 .. +145
	parent1ROTATE = AddAnimationComponent(anim_SPANEL1ROTATE, 0, 1, SOLAR2, parent1);


	static UINT sail3[4] = { GRP_PANEL1HINGE1,GRP_PANEL1ARM1,GRP_PANEL1ARM2,GRP_PANEL1ARM3 };//sa_s1
	SOLAR3 = new MGROUP_ROTATE(2, sail3, 4, _V(1.5338, 1.481138, -1.679126), _V(1, 0, 0), (float)(90 * RAD)); // -2 .. +145
	parent1 = AddAnimationComponent(anim_SPANELFOLD, .5, 1, SOLAR3, parent1ROTATE);


	static UINT sail4[4] = { GRP_PANEL1HINGE2,GRP_PANEL1HINGE3,GRP_PANEL1B,GRP_PANEL1A };//sa_s1
	SOLAR4 = new MGROUP_ROTATE(2, sail4, 4, _V(1.707556, 1.506648, -2.222587), _V(1, 0, 0), (float)(-180 * RAD)); // -2 .. +145
	parent1 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR4, parent1);
	
	static UINT sail5[4] = { GRP_PANEL1HINGE4,GRP_PANEL1HINGE5,GRP_PANEL1C,GRP_PANEL1D };//sa_s1
	SOLAR5 = new MGROUP_ROTATE(2, sail5, 4, _V(1.67655, 1.540389, .1210343), _V(1, 0, 0), (float)(180 * RAD)); // -2 .. +145
	parent1 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR5, parent1);

	static UINT sail6[5] = { GRP_PANEL1HINGE6,GRP_PANEL1E,GRP_PANEL1F ,GRP_PANEL1SUPPORT1 ,GRP_PANEL1SUPPORT2 };//sa_s1
	SOLAR6 = new MGROUP_ROTATE(2, sail6, 5, _V(1.707556, 1.558236, -2.201092), _V(1, 0, 0), (float)(-180 * RAD)); // -2 .. +145
	parent1 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR6, parent1);






	SOLAR5A = new MGROUP_ROTATE(LOCALVERTEXLIST, MAKEGROUPARRAY(arm_tip), 3, PANEL1CAM_POS, _V(0, 0, 1), (float)(0.0));
	anim_SPANEL1CAM = CreateAnimation(0.0);
	parent1 = AddAnimationComponent(anim_SPANEL1CAM, 0, 1, SOLAR5A, parent1);




	//SOLAR PANEL 2
	static UINT SPANELGrp1[1] = { GRP_PANEL2PARENT };
	static MGROUP_ROTATE SPANEL2(2, SPANELGrp1, 1, _V(-1.724162, .6841645, -1.6553), _V(0, 0, 1), (float)(60 * RAD));
	parent2A = AddAnimationComponent(anim_SPANELROT, 0, 1, &SPANEL2);

	static UINT SPANELGrp2[2] = { GRP_PANEL2BASE1,GRP_PANEL2BASE2 };
	SOLAR7 = new MGROUP_ROTATE(2, SPANELGrp2, 2, _V(-1.7349, .8818773, -1.676777), _V(1, 0, 0), (float)(120 * RAD));
	parent2 = AddAnimationComponent(anim_SPANEL, 0, 1, SOLAR7, parent2A);

	static UINT SPANELGrp3[1] = { GRP_PANEL2ROTATE };
	SOLAR8 = new MGROUP_ROTATE(2, SPANELGrp3, 1, _V(-1.459, 1.112, -1.677), _V(0, 1, 0), (float)(-360 * RAD)); // -2 .. +145
	parent2ROTATE = AddAnimationComponent(anim_SPANEL2ROTATE, 0, 1, SOLAR8, parent2);

	static UINT sail7[4] = { GRP_PANEL2HINGE1,GRP_PANEL2ARM1,GRP_PANEL2ARM2,GRP_PANEL2ARM3 };//sa_s1
	SOLAR9 = new MGROUP_ROTATE(2, sail7, 4, _V(-1.458694, 1.018378, -1.679238), _V(1, 0, 0), (float)(90 * RAD)); // -2 .. +145
	parent2 = AddAnimationComponent(anim_SPANELFOLD, .5, 1, SOLAR9, parent2ROTATE);

	static UINT sail8[4] = { GRP_PANEL2HINGE2,GRP_PANEL2HINGE3,GRP_PANEL2B,GRP_PANEL2A };//sa_s1
	SOLAR10 = new MGROUP_ROTATE(2, sail8, 4, _V(-1.459136, 1.046272, -2.219137), _V(1, 0, 0), (float)(-180 * RAD)); // -2 .. +145
	parent2 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR10, parent2);

	static UINT sail9[4] = { GRP_PANEL2HINGE4,GRP_PANEL2HINGE5,GRP_PANEL2C,GRP_PANEL2D };//sa_s1
	SOLAR11 = new MGROUP_ROTATE(2, sail9, 4, _V(-1.490444, 1.077327, .120681), _V(1, 0, 0), (float)(180 * RAD)); // -2 .. +145
	parent2 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR11, parent2);






	static UINT sail10[5] = { GRP_PANEL2HINGE6,GRP_PANEL2E,GRP_PANEL2F ,GRP_PANEL2SUPPORT1 ,GRP_PANEL2SUPPORT2 };//sa_s1
	SOLAR11A = new MGROUP_ROTATE(2, sail10, 5, _V(-1.459136, 1.096141, -2.200221), _V(1, 0, 0), (float)(-180 * RAD)); // -2 .. +145
	parent2 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR11A, parent2);


	SOLAR10A = new MGROUP_ROTATE(LOCALVERTEXLIST, MAKEGROUPARRAY(arm_tip1), 3, PANEL2CAM_POS, _V(0, 0, 1), (float)(0.0));
	anim_SPANEL2CAM = CreateAnimation(0.0);
	parent2 = AddAnimationComponent(anim_SPANEL2CAM, 0, 1, SOLAR10A, parent2);
	
	//wing3
	//
	static UINT SPANEL3Grp1[1] = { GRP_PANEL3PARENT };
	static MGROUP_ROTATE SPANEL3(2, SPANEL3Grp1, 1, _V(-1.443768, -1.140396, -1.655312), _V(0, 0, 1), (float)(-60 * RAD));
	parent3A = AddAnimationComponent(anim_SPANELROT, 0, 1, &SPANEL3);

	static UINT SPANEL3Grp2[2] = { GRP_PANEL3BASE1,GRP_PANEL3BASE2 };
	SOLAR11 = new MGROUP_ROTATE(2, SPANEL3Grp2, 2, _V(-1.533802, -1.335799, -1.677047), _V(1, 0, 0), (float)(-120 * RAD));
	parent3 = AddAnimationComponent(anim_SPANEL, 0, 1, SOLAR11, parent3A);

	static UINT SPANEL3Grp3[1] = { GRP_PANEL3ROTATE };
	SOLAR12 = new MGROUP_ROTATE(2, SPANEL3Grp3, 1, _V(-1.71, -1.113, -1.678), _V(0, 1, 0), (float)(-360 * RAD)); // -2 .. +145
	parent3ROTATE = AddAnimationComponent(anim_SPANEL3ROTATE, 0, 1, SOLAR12, parent3);


	static UINT sail11[4] = { GRP_PANEL3HINGE1,GRP_PANEL3ARM1,GRP_PANEL3ARM2 ,GRP_PANEL3ARM3 };//sa_s1
	SOLAR13 = new MGROUP_ROTATE(2, sail11, 4, _V(-1.709859, -1.472176, -1.678603), _V(1, 0, 0), (float)(-90 * RAD)); // -2 .. +145
	parent3 = AddAnimationComponent(anim_SPANELFOLD, .5, 1, SOLAR13, parent3ROTATE);

	static UINT sail12[4] = { GRP_PANEL3HINGE2,GRP_PANEL3HINGE3,GRP_PANEL3B,GRP_PANEL3A };//sa_s1
	SOLAR14 = new MGROUP_ROTATE(2, sail12, 4, _V(-1.709859, -1.501488, -2.221749), _V(1, 0, 0), (float)(180 * RAD)); // -2 .. +145
	parent3 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR14, parent3);

	static UINT sail13[4] = { GRP_PANEL3HINGE4,GRP_PANEL3HINGE5,GRP_PANEL3C,GRP_PANEL3D };//sa_s1
	SOLAR15 = new MGROUP_ROTATE(2, sail13, 4, _V(-1.676867, -1.535023, .1177467), _V(1, 0, 0), (float)(-180 * RAD)); // -2 .. +145
	parent3 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR15, parent3);

	static UINT sail14[5] = { GRP_PANEL3HINGE6,GRP_PANEL3E,GRP_PANEL3F,GRP_PANEL3SUPPORT1 ,GRP_PANEL3SUPPORT2 };//sa_s1
	SOLAR16 = new MGROUP_ROTATE(2, sail14, 5, _V(-1.709859, -1.553217, -2.201058), _V(1, 0, 0), (float)(180 * RAD)); // -2 .. +145
	parent3 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR16, parent3);


	SOLAR15A = new MGROUP_ROTATE(LOCALVERTEXLIST, MAKEGROUPARRAY(arm_tip2), 3, PANEL3CAM_POS, _V(0, 0, 1), (float)(0.0));
	anim_SPANEL3CAM = CreateAnimation(0.0);
	parent3 = AddAnimationComponent(anim_SPANEL3CAM, 0, 1, SOLAR15A, parent3);

	

	//4

	//WING4

	//
	static UINT SPANEL4Grp1[1] = { GRP_PANEL4PARENT };
	static MGROUP_ROTATE SPANEL4(2, SPANEL4Grp1, 1, _V(1.714099, -.6820283, -1.655311), _V(0, 0, 1), (float)(60 * RAD));
	parent4A = AddAnimationComponent(anim_SPANELROT, 0, 1, &SPANEL4);

	static UINT SPANEL4Grp3[2] = { GRP_PANEL4BASE1,GRP_PANEL4BASE2 };
	SOLAR18 = new MGROUP_ROTATE(2, SPANEL4Grp3, 2, _V(1.624397, -.8778392, -1.677047), _V(1, 0, 0), (float)(-120 * RAD));
	parent4 = AddAnimationComponent(anim_SPANEL, 0, 1, SOLAR18, parent4A);


	static UINT SPANEL4Grp2[1] = { GRP_PANEL4ROTATE };
	SOLAR17 = new MGROUP_ROTATE(2, SPANEL4Grp2, 1, _V(1.449, -.9010087, -1.68), _V(0, 1, 0), (float)(-360 * RAD));
	parent4ROTATE = AddAnimationComponent(anim_SPANEL4ROTATE, 0, 1, SOLAR17, parent4);


	

	static UINT sail15[4] = { GRP_PANEL4HINGE1,GRP_PANEL4ARM1,GRP_PANEL4ARM2,GRP_PANEL4ARM3 };//sa_s1
	SOLAR18 = new MGROUP_ROTATE(2, sail15, 4, _V(1.449796, -1.014619, -1.678597), _V(1, 0, 0), (float)(-90 * RAD)); // -2 .. +145
	parent4 = AddAnimationComponent(anim_SPANELFOLD, .5, 1, SOLAR18, parent4ROTATE);
	
	static UINT sail16[4] = { GRP_PANEL4HINGE2,GRP_PANEL4HINGE3,GRP_PANEL4B,GRP_PANEL4A };//sa_s1
	SOLAR19 = new MGROUP_ROTATE(2, sail16, 4, _V(1.449927, -1.046475, -2.220004), _V(1, 0, 0), (float)(180 * RAD)); // -2 .. +145
	parent4 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR19, parent4);
	
	static UINT sail17[4] = { GRP_PANEL4HINGE4,GRP_PANEL4HINGE5,GRP_PANEL4C,GRP_PANEL4D };//sa_s1
	SOLAR20 = new MGROUP_ROTATE(2, sail17, 4, _V(1.412395, -1.075065, .1188283), _V(1, 0, 0), (float)(-180 * RAD)); // -2 .. +145
	parent4 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR20, parent4);
	
	static UINT sail18[5] = { GRP_PANEL4HINGE6,GRP_PANEL4E,GRP_PANEL4F,GRP_PANEL4SUPPORT1 ,GRP_PANEL4SUPPORT2 };//sa_s1
	SOLAR21 = new MGROUP_ROTATE(2, sail18, 5, _V(1.449927, -1.094622, -2.201089), _V(1, 0, 0), (float)(180 * RAD)); // -2 .. +145
	parent4 = AddAnimationComponent(anim_SPANELFOLD, 0, 1, SOLAR21, parent4);





	SOLAR21A = new MGROUP_ROTATE(LOCALVERTEXLIST, MAKEGROUPARRAY(arm_tip3), 3, PANEL4CAM_POS, _V(0, 0, 1), (float)(0.0));
	anim_SPANEL4CAM = CreateAnimation(0.0);
	parent4 = AddAnimationComponent(anim_SPANEL4CAM, 0, 1, SOLAR21A, parent4);
	
}


// --------------------------------------------------------------
// Keyboard interface handler (buffered key events)
// --------------------------------------------------------------
int ARTEMIS2::clbkConsumeBufferedKey(DWORD key, bool down, char* kstate)
{
	// only process keydown events
	if (!down)
		return 0;
	if (key == OAPI_KEY_G)
	{
		RevertHATCH();
	
		return 1;
	}
	if (key == OAPI_KEY_2)
	{
		if (panelstow == 0) {
			panelstow = 1;
			center_arm_t = oapiGetSimTime();
		}


		return 1;
	}
	if ((key == OAPI_KEY_5) && KEYMOD_SHIFT(kstate))
	{
		RevertTABLE();
		return 1;
	}
	if (key == OAPI_KEY_0)
	{
		if (lightON == 0)lightON = 1;
		else lightON = 0;

		return 1;
	}
	if (key == OAPI_KEY_1)
	{
		RevertSPANELFOLD();
		return 1;
	}
	if ((key == OAPI_KEY_5) && KEYMOD_CONTROL(kstate))//change crew
	{
		if (CHAIRDEPLOY > 2)CHAIRDEPLOY = 0;
		CHAIRDEPLOY = CHAIRDEPLOY + 1;
		if (CHAIRDEPLOY > 2)CHAIRDEPLOY = 0;
		

		return 1;
	}
	if (key == OAPI_KEY_6 && !KEYMOD_SHIFT(kstate) && !KEYMOD_ALT(kstate))
	{ // operate panels fore/aft
		if (ARM2_check == SPANEL_STOP)ARM2_check = SPANEL_UP;
		else if (ARM2_check == SPANEL_UP)ARM2_check = SPANEL_STOP;
		else if (ARM2_check == SPANEL_DOWN)ARM2_check = SPANEL_UP;
		return 1;
	}
	if (key == OAPI_KEY_7 && !KEYMOD_SHIFT(kstate) && !KEYMOD_ALT(kstate))
	{ // operate panels for aft
		if (ARM2_check == SPANEL_STOP)ARM2_check = SPANEL_DOWN;
		else if (ARM2_check == SPANEL_DOWN)ARM2_check = SPANEL_STOP;
		else if (ARM2_check == SPANEL_UP)ARM2_check = SPANEL_DOWN;
		return 1;
	}

	if (key == OAPI_KEY_K)
	{
		if (solp_rot_status == 1) solp_rot_status = 2;
		else if (solp_rot_status == 2) solp_rot_status = 1;
		else if (solp_rot_status == 3) solp_rot_status = 1;

		return 1;
	}

	if (key == OAPI_KEY_3)
	{
		if (BEACON == 0)BEACON = 1;
		else (BEACON = 0);

	}
	if (key == OAPI_KEY_4)
	{
		if (CAM > 4)CAM = 0;
		CAM = CAM + 1;
		if (CAM > 4)CAM = 0;
		return 1;
	}

	return 0;

}

// ====================================================================
// clbkVisualCreated used to display UMMU initialisation message 
// because oapiDebugString() doesn't work in clbkSetClassCap
// ====================================================================

// ==============================================================
// Visual destroyed
// ==============================================================
void ARTEMIS2::clbkVisualDestroyed(VISHANDLE vis, int refcount)
{
	MainExternalMeshVisual = 0;
}


void ARTEMIS2::clbkSaveState(FILEHANDLE scn)
{
	char cbuf[256];

	

		oapiWriteScenario_int(scn, "MODE", mode);

		sprintf(cbuf, "%d", BEACON);
		oapiWriteScenario_string(scn, "BEACON", cbuf);


		sprintf(cbuf, "%d", BLOWNFAIRING);
		oapiWriteScenario_string(scn, "BLOWNFAIRING", cbuf);

		sprintf(cbuf, "%d", CHAIRDEPLOY);
		oapiWriteScenario_string(scn, "CHAIR", cbuf);

		sprintf(cbuf, "%d %0.4f", TABLE_status, TABLE_proc);
		oapiWriteScenario_string(scn, "HATCH", cbuf);


		sprintf(cbuf, "%d %0.4f", HATCH_status, HATCH_proc);
		oapiWriteScenario_string(scn, "TABLE", cbuf);

		if (fairing && mode > 9)
			oapiWriteScenario_int(scn, "FAIRING", mode);



		sprintf(cbuf, "%d %0.4f", SPANELFOLD_status, SPANELFOLD_proc);
		oapiWriteScenario_string(scn, "PANELFOLD", cbuf);

		sprintf(cbuf, "%0.4f", JOINT1_proc);
		oapiWriteScenario_string(scn, "JOINT1", cbuf);
		sprintf(cbuf, "%0.4f", WING1ROTATE_proc);
		oapiWriteScenario_string(scn, "WING1ROTATE", cbuf);
		sprintf(cbuf, "%0.4f", WING2ROTATE_proc);
		oapiWriteScenario_string(scn, "WING2ROTATE", cbuf);

		sprintf(cbuf, "%0.4f", WING3ROTATE_proc);
		oapiWriteScenario_string(scn, "WING3ROTATE", cbuf);

		sprintf(cbuf, "%0.4f", WING4ROTATE_proc);
		oapiWriteScenario_string(scn, "WING4ROTATE", cbuf);

		sprintf(cbuf, "%d %0.4f %d %0.4f", solp_status, solp_proc, solp_rot_status, solp_rot_proc);
		oapiWriteScenario_string(scn, "SOLP", cbuf);


		


	SaveDefaultState(scn);

}
	


void ARTEMIS2::clbkLoadStateEx(FILEHANDLE scn, void* status)
{
	char* line;
	int value;
	while (oapiReadScenario_nextline(scn, line))
	{
		
		{
			if (!_strnicmp(line, "MODE", 4)) {
				sscanf_s(line + 4, "%d", &value);
				mode = (int)value;
			}


			if (!_strnicmp(line, "FAIRING", 7)) {
				fairing = 1;
			}
			if (!_strnicmp(line, "BEACON", 6)) {
				if (!_strnicmp(line, "BEACON", 6)) {
					sscanf(line + 6, "%d ", &BEACON);
				}
				sscanf(line + 6, "%d ", &BEACON);
			}

			if (!_strnicmp(line, "CHAIR", 5)) {
				sscanf(line + 5, "%d ", &CHAIRDEPLOY);
			}

			if (!_strnicmp(line, "BLOWNFAIRING", 12)) {
				sscanf(line + 12, "%d ", &BLOWNFAIRING);
			}


			if (!_strnicmp(line, "HATCH", 5)) {
				sscanf(line + 5, "%d%lf", &HATCH_status, &HATCH_proc);
			}

			if (!_strnicmp(line, "TABLE", 5)) {
				sscanf(line + 5, "%d%lf", &TABLE_status, &TABLE_proc);
			}

			if (!_strnicmp(line, "PANELFOLD", 9)) {
				sscanf(line + 9, "%d%lf", &SPANELFOLD_status, &SPANELFOLD_proc);
			}

			if (!_strnicmp(line, "JOINT1", 6)) {
				sscanf(line + 6, "%lf", &JOINT1_proc);
			}

			if (!_strnicmp(line, "WING1ROTATE", 11)) {
				sscanf(line + 11, "%lf", &WING1ROTATE_proc);
			}

			if (!_strnicmp(line, "WING2ROTATE", 11)) {
				sscanf(line + 11, "%lf", &WING2ROTATE_proc);
			}
			if (!_strnicmp(line, "WING3ROTATE", 11)) {
				sscanf(line + 11, "%lf", &WING3ROTATE_proc);
			}
			if (!_strnicmp(line, "WING4ROTATE", 11)) {
				sscanf(line + 11, "%lf", &WING4ROTATE_proc);
			}


			if (!_strnicmp(line, "SOLP", 4)) {
				sscanf(line + 4, "%d%lf%d%lf", &solp_status, &solp_proc, &solp_rot_status, &solp_rot_proc);
			}


			switch (mode) {
			case 0:
				SetConfig0_Ready();
				break;
			case 1:
				SetConfig1_Orbit();
				break;
			case 2:
				SetConfig2_Reentry();
				break;
			case 3:
				SetConfig3_Drogue();
				break;
			case 4:
			case 5:
				SetConfig4_Chute();
				break;
			case 6:
				SetConfig6_Land();
				break;





			case 10:
				SetConfig10_Launch0();


				break;
			case 11:
				SetConfig11_Launch1();
				break;
			}








			ParseScenarioLineEx(line, status);
		}

		SetAnimation(anim_SPANELFOLD, SPANELFOLD_proc);
		SetAnimation(anim_HATCH, HATCH_proc);
		SetAnimation(anim_TABLE, TABLE_proc);
		SetAnimation(anim_SPANEL1ROTATE, WING1ROTATE_proc);
		SetAnimation(anim_SPANEL2ROTATE, WING2ROTATE_proc);
		SetAnimation(anim_SPANEL3ROTATE, WING3ROTATE_proc);
		SetAnimation(anim_SPANEL4ROTATE, WING4ROTATE_proc);
		SetAnimation(anim_SPANEL, JOINT1_proc);
	}

}

DLLCLBK void InitModule(HINSTANCE hModule)
{
	
		g_Param.hDLL = hModule;

		g_Param.font[0] = CreateFont(36, 16, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");

		g_Param.font[1] = CreateFont(70, 0, 0, 0, 0, 000, 0, 0, 0, 0, 0, 0, 0, "Seven Segment");

		g_Param.col[4] = RGB(0, 153, 0);

		g_Param.hBrush[1] = CreateSolidBrush(g_Param.col[4]);
	


	
}

DLLCLBK void ExitModule(HINSTANCE hModule)
{
	oapiUnregisterCustomControls(hModule);
	oapiDestroySurface(g_Param.tkbk_label);

	// deallocate GDI resources
	DeleteObject(g_Param.font[0]);
}


void ARTEMIS2::RevertHATCH(void)
{
	HATCH_status = ((HATCH_status == HATCH_UP || HATCH_status == HATCH_RAISING) ?
		HATCH_LOWERING : HATCH_RAISING);
}
void ARTEMIS2::RevertSPANELFOLD(void)
{
	SPANELFOLD_status = ((SPANELFOLD_status == HATCH_UP || SPANELFOLD_status == HATCH_RAISING) ?
		HATCH_LOWERING : HATCH_RAISING);
}
void ARTEMIS2::RevertTABLE(void)
{
	TABLE_status = ((TABLE_status == HATCH_UP || TABLE_status == HATCH_RAISING) ?
		HATCH_LOWERING : HATCH_RAISING);
}


void ARTEMIS2::clbkMFDMode(int mfd, int mode)
{
	oapiVCTriggerRedrawArea(-1, ALL_MFD_BUTTONS);
}

bool ARTEMIS2::clbkLoadVC(int id)
{
	SetCameraOffset(_V(-0.41, 1.0, 2.241));
	SetCameraDefaultDirection(_V(0, -0.1908, 0.9816));

	DefineMFDButtons();
	oapiVCRegisterMFD(MFD_LEFT, &cdr1_mfd);
	oapiVCRegisterMFD(MFD_RIGHT, &plt1_mfd);
	oapiVCRegisterMFD(MFD_USER1, &cntr1_mfd);

	//left, right, top bottom

	switch (id) {
	case 0: // commander position
		SetCameraOffset(_V(-0.41, 1.0, 2.441));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.2), 0, 0, _V(-0.3, 0, 0), 75 * RAD, -5 * RAD, _V(0.3, 0, 0), -20 * RAD, -27 * RAD);
		oapiVCSetNeighbours(6, 1, 4, 3);
		vccameracase = 0;

		break;

	case 1: // center mfd
		SetCameraOffset(_V(0, 1.0, 2.441));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(0, 2, 8, 3);
		vccameracase = 2;

		break;
	case 2: // pilot position
		SetCameraOffset(_V(0.41, 1.0, 2.441));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(1, 7, 5, 3);
		vccameracase = 1;

		break;
	case 3: // dock tunnel
		SetCameraOffset(_V(0, .2, 2.64));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(6, 7, 1, -1);
		vccameracase = 3;
		break;
	case 4: //dock window

		SetCameraOffset(_V(-0.41, 1.4, 2.241));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(-1, 8, -1, 0);
		vccameracase = 4;
		break;
	case 5:

		SetCameraOffset(_V(0.41, 1.4, 2.241));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(8, -1, -1, 2);
		vccameracase = 4;
		break;
	case 6://galley

		SetCameraOffset(_V(-0.91, .8, 2.94));
		SetCameraDefaultDirection(_V(0, 0, -1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(-1, 3, 0, -1);
		vccameracase = 4;
		break;
	case 7://bad

		SetCameraOffset(_V(.69, 1.10, 2.74));
		SetCameraDefaultDirection(_V(0, 0, -1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(3, -1, 2, -1);
		vccameracase = 4;
		break;
	case 8:

		SetCameraOffset(_V(0, 1.4, 2.04));
		SetCameraDefaultDirection(_V(0, 0, 1));
		SetCameraMovement(_V(0, 0, 0.3), 0, 0, _V(-0.3, 0, 0), 20 * RAD, -27 * RAD, _V(0.3, 0, 0), -75 * RAD, -5 * RAD);
		oapiVCSetNeighbours(4, 5, -1, 1);
		vccameracase = 4;
		break;
	}





	return true;
}
//-------------------------------------------------------------------------

bool ARTEMIS2::clbkVCMouseEvent(int id, int event, VECTOR3& p)
{

	if (id == CDR1_PWR) {
		oapiSendMFDKey(MFD_LEFT, OAPI_KEY_ESCAPE);
		return true;
	}
	if (id == CDR1_SEL) {
		oapiSendMFDKey(MFD_LEFT, OAPI_KEY_F1);
		return true;
	}
	if (id == CDR1_MNU) {
		oapiSendMFDKey(MFD_LEFT, OAPI_KEY_GRAVE);
		return true;
	}
	if (id == PLT1_PWR) {
		oapiSendMFDKey(MFD_RIGHT, OAPI_KEY_ESCAPE);
		return true;
	}
	if (id == PLT1_SEL) {
		oapiSendMFDKey(MFD_RIGHT, OAPI_KEY_F1);
		return true;
	}
	if (id == PLT1_MNU) {
		oapiSendMFDKey(MFD_RIGHT, OAPI_KEY_GRAVE);
		return true;
	}
	if (id == CNTR1_PWR) {
		oapiSendMFDKey(MFD_USER1, OAPI_KEY_ESCAPE);
		return true;
	}
	if (id == CNTR1_SEL) {
		oapiSendMFDKey(MFD_USER1, OAPI_KEY_F1);
		return true;
	}
	if (id == CNTR1_MNU) {
		oapiSendMFDKey(MFD_USER1, OAPI_KEY_GRAVE);
		return true;
	}
	if (id == CDR1_LBUTTONS) {
		int button = p.y * 6;
		oapiProcessMFDButton(MFD_LEFT, button, event);
		return true;
	}
	if (id == CDR1_RBUTTONS) {
		int button = p.y * 6;
		oapiProcessMFDButton(MFD_LEFT, button + 6, event);
		return true;
	}
	if (id == PLT1_LBUTTONS) {
		int button = p.y * 6;
		oapiProcessMFDButton(MFD_RIGHT, button, event);
		return true;
	}
	if (id == PLT1_RBUTTONS) {
		int button = p.y * 6;
		oapiProcessMFDButton(MFD_RIGHT, button + 6, event);
		return true;
	}
	if (id == CNTR1_LBUTTONS) {
		int button = p.y * 6;
		oapiProcessMFDButton(MFD_USER1, button, event);
		return true;
	}
	if (id == CNTR1_RBUTTONS) {
		int button = p.y * 6;
		oapiProcessMFDButton(MFD_USER1, button + 6, event);
		return true;
	}
	if (id == AID_METMODE) {
		METmode = 1;
		GMTmode = 0;
		SetAnimation(anim_TIMERKNOB, 0);
		return true;
	}
	if (id == AID_GMTMODE) {
		METmode = 0;
		GMTmode = 1;
		SetAnimation(anim_TIMERKNOB, 1);
		return true;
	}




	return false;
}





bool ARTEMIS2::clbkVCRedrawEvent(int id, int event, SURFHANDLE surf)
{
	bool return_value = false;
	if (id == ALL_MFD_BUTTONS) {

		PaintMFDButtons(surf);
		return_value = true;
	}
	if (id == AID_METTIME) {
		RedrawPanel_metmeterstatus(surf, id);
		return true;
	}

	return return_value;
}
void ARTEMIS2::PaintMFDButtons(SURFHANDLE surf)
{
	HDC hDC = oapiGetDC(surf);
	for (int mfd = 0; mfd < 3; mfd++) {
		// D. Beachy: BUGFIX: if MFD powered off, do not paint buttons
		if (oapiGetMFDMode(mfd) == MFD_NONE) {
		}
		else
		{ // MFD powered on
			//sprintf(oapiDebugString(), "MFD MODE %d MFD %d ", oapiGetMFDMode(mfd), mfd);
			HFONT pFont = (HFONT)SelectObject(hDC, g_Param.font[0]);
			SetTextColor(hDC, RGB(255, 255, 255));
			SetTextAlign(hDC, TA_CENTER);
			SetBkMode(hDC, TRANSPARENT);
			const char* label;
			int y = 16;
			for (int bt = 0; bt < 6; bt++) {
				if (label = oapiMFDButtonLabel(mfd, bt)) {
					TextOut(hDC, mfd_buttons_ypos[mfd], y, label, strlen(label));
					y += 64;
				}
				else break;
			}
			int y2 = 16;
			for (int bt = 6; bt < 12; bt++) {
				if (label = oapiMFDButtonLabel(mfd, bt)) {
					TextOut(hDC, mfd_buttons_ypos[mfd] + 128, y2, label, strlen(label));
					y2 += 64;
				}
				else break;
			}
			//TextOut (hDC, 234, 1, "PG", 2);
			SelectObject(hDC, pFont);
		}
	}
	oapiReleaseDC(surf, hDC);
}
void ARTEMIS2::DefineMFDButtons()
{
	//SURFHANDLE mfd_buttons_surf;
	//mfd_buttons_surf = oapiGetTextureHandle(VCMeshUINT, 11);
	//SURFHANDLE tex1 = oapiGetTextureHandle (hMcap, 7);

	SURFHANDLE mfd_buttons_surf = oapiGetTextureHandle(VCMESH, 11);

	oapiVCRegisterArea(CDR1_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(CDR1_SEL, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(CDR1_MNU, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral(CDR1_PWR, CDR1_PWR_AREA.pt1, CDR1_PWR_AREA.pt2, CDR1_PWR_AREA.pt3, CDR1_PWR_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(CDR1_SEL, CDR1_SEL_AREA.pt1, CDR1_SEL_AREA.pt2, CDR1_SEL_AREA.pt3, CDR1_SEL_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(CDR1_MNU, CDR1_MNU_AREA.pt1, CDR1_MNU_AREA.pt2, CDR1_MNU_AREA.pt3, CDR1_MNU_AREA.pt4);

	oapiVCRegisterArea(PLT1_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(PLT1_SEL, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(PLT1_MNU, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral(PLT1_PWR, PLT1_PWR_AREA.pt1, PLT1_PWR_AREA.pt2, PLT1_PWR_AREA.pt3, PLT1_PWR_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(PLT1_SEL, PLT1_SEL_AREA.pt1, PLT1_SEL_AREA.pt2, PLT1_SEL_AREA.pt3, PLT1_SEL_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(PLT1_MNU, PLT1_MNU_AREA.pt1, PLT1_MNU_AREA.pt2, PLT1_MNU_AREA.pt3, PLT1_MNU_AREA.pt4);

	oapiVCRegisterArea(CNTR1_PWR, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(CNTR1_SEL, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(CNTR1_MNU, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral(CNTR1_PWR, CNTR1_PWR_AREA.pt1, CNTR1_PWR_AREA.pt2, CNTR1_PWR_AREA.pt3, CNTR1_PWR_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(CNTR1_SEL, CNTR1_SEL_AREA.pt1, CNTR1_SEL_AREA.pt2, CNTR1_SEL_AREA.pt3, CNTR1_SEL_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(CNTR1_MNU, CNTR1_MNU_AREA.pt1, CNTR1_MNU_AREA.pt2, CNTR1_MNU_AREA.pt3, CNTR1_MNU_AREA.pt4);

	oapiVCRegisterArea(CDR1_LBUTTONS, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBUP | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(CDR1_RBUTTONS, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBUP | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral(CDR1_LBUTTONS, CDR1_LBUTTONS_AREA.pt1, CDR1_LBUTTONS_AREA.pt2, CDR1_LBUTTONS_AREA.pt3, CDR1_LBUTTONS_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(CDR1_RBUTTONS, CDR1_RBUTTONS_AREA.pt1, CDR1_RBUTTONS_AREA.pt2, CDR1_RBUTTONS_AREA.pt3, CDR1_RBUTTONS_AREA.pt4);

	oapiVCRegisterArea(PLT1_LBUTTONS, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBUP | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(PLT1_RBUTTONS, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBUP | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral(PLT1_LBUTTONS, PLT1_LBUTTONS_AREA.pt1, PLT1_LBUTTONS_AREA.pt2, PLT1_LBUTTONS_AREA.pt3, PLT1_LBUTTONS_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(PLT1_RBUTTONS, PLT1_RBUTTONS_AREA.pt1, PLT1_RBUTTONS_AREA.pt2, PLT1_RBUTTONS_AREA.pt3, PLT1_RBUTTONS_AREA.pt4);

	oapiVCRegisterArea(CNTR1_LBUTTONS, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBUP | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCRegisterArea(CNTR1_RBUTTONS, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBUP | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Quadrilateral(CNTR1_LBUTTONS, CNTR1_LBUTTONS_AREA.pt1, CNTR1_LBUTTONS_AREA.pt2, CNTR1_LBUTTONS_AREA.pt3, CNTR1_LBUTTONS_AREA.pt4);
	oapiVCSetAreaClickmode_Quadrilateral(CNTR1_RBUTTONS, CNTR1_RBUTTONS_AREA.pt1, CNTR1_RBUTTONS_AREA.pt2, CNTR1_RBUTTONS_AREA.pt3, CNTR1_RBUTTONS_AREA.pt4);


	oapiVCRegisterArea(AID_CNTR_BRT, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	//oapiVCSetAreaClickmode_Quadrilateral(AID_CNTR_BRT, _V(-.135624,  .8397668, 2.817692), _V(-.541, 1.085916, 2.737), _V(-.5574965, 1.075916, 2.737), _V(-.541, 1.075916, 2.737));
	oapiVCSetAreaClickmode_Spherical(AID_CNTR_BRT, _V(-.135624, .8397668, 2.817692), .05);


	oapiVCRegisterArea(AID_CNTR_DIM, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	//oapiVCSetAreaClickmode_Quadrilateral(AID_CNTR_DIM, _V(-.5484657, .8191087, 2.817692), _V(-.541, 1.06478, 2.7802), _V(-.5561106, 1.05478, 2.7802), _V(-.541, 1.05478, 2.7802));
	oapiVCSetAreaClickmode_Spherical(AID_CNTR_DIM, _V(-.1353547, .818491, 2.817692), .05);



	oapiVCRegisterArea(ALL_MFD_BUTTONS, _R(0, 0, 1536, 384), PANEL_REDRAW_USER, PANEL_MOUSE_IGNORE, PANEL_MAP_BACKGROUND, mfd_buttons_surf);


	SURFHANDLE const tex3 = oapiGetTextureHandle(VCMESH, 14);
	oapiVCRegisterArea(AID_METTIME, _R(0, 0, 512, 512), PANEL_REDRAW_USER, PANEL_MOUSE_IGNORE, PANEL_MAP_BACKGROUND, tex3);

	oapiVCRegisterArea(AID_METMODE, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Spherical(AID_METMODE, _V(.027, 1.232, 2.79), .05);

	oapiVCRegisterArea(AID_GMTMODE, PANEL_REDRAW_NEVER, PANEL_MOUSE_LBDOWN | PANEL_MOUSE_LBPRESSED | PANEL_MOUSE_ONREPLAY);
	oapiVCSetAreaClickmode_Spherical(AID_GMTMODE, _V(.027, 1.191, 2.79), .05);
}
void ARTEMIS2::RedrawPanel_metmeterstatus(SURFHANDLE surf, int part)

{
	//SURFHANDLE const tex3 = oapiGetTextureHandle(METNUMBERS,1 );
	//SURFHANDLE const tex3 = oapiGetTextureHandle(hOrbiterVCMesh, 15);

	char cbuf[20];
	HDC hDC = oapiGetDC(surf);






	SelectObject(hDC, g_Param.hBrush[1]);//green box
	//Rectangle(hDC, 0, 144, (int)(150  * (351 - 150)), 187);//thrust
	//Rectangle(hDC, 75, 72, (int)(75  * (175 - 75)), 92);//thrust
//	Rectangle(hDC, 150, 217, (int)(150 + level3 * (350 - 150)), 260);//thrust
	//Rectangle(hDC, 75, 109, (int)(75 + level3 * (175 - 75)), 129);// hoverthrust

	//Rectangle(hDC, 152, 70, (int)(152.0 + (level2 / 20.2)), 108);  //fuel  RANGE 175
	//Rectangle(hDC, 75, 35, (int)(75 + (level2 / 40.2)), 55);  //fuel  RANGE 175

	//Rectangle(hDC, 312, (int)(292 - (48.5* trim2)), 333, (int)(298 - (48.5* trim2)));  //TRIM
//	Rectangle(hDC, 156, (int)(146 - (24.25 * trim2)), 166, (int)(149 - (24.25 * trim2)));  //TRIM

	SelectObject(hDC, g_Param.font[1]);
	SetTextColor(hDC, RGB(0, 153, 0));
	SetTextAlign(hDC, TA_LEFT);
	SetBkMode(hDC, TRANSPARENT);

	sprintf(cbuf, "%d", timer_state[8]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 75, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[7]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 105, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[6]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 135, 30, cbuf, strlen(cbuf));

	sprintf(cbuf, "%d", timer_state[5]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 180, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[4]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 210, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[3]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 255, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[2]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 285, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[1]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 340, 30, cbuf, strlen(cbuf));
	sprintf(cbuf, "%d", timer_state[0]);
	//	TextOut(hDC, 410, 325, cbuf, strlen(cbuf));
	TextOut(hDC, 370, 30, cbuf, strlen(cbuf));

	oapiReleaseDC(surf, hDC);


}

VECTOR3 ARTEMIS2::hms(double time) {
	VECTOR3 met;
	if (time == 0) { met = _V(0, 0, 0); }
	else {
		time = abs(time + 0.5 * (time / abs(time) - 1));
		met.x = floor(time / 3600) - 0.5 * (time / abs(time) - 1);
		met.y = floor((time - met.x * 3600 * (time / abs(time))) / 60) - 0.5 * (time / abs(time) - 1);
		met.z = floor(time - met.x * 3600 * (time / abs(time)) - met.y * 60 * (time / abs(time)));
	}
	return met;
}

void ARTEMIS2::SetConfig0_Ready(void)
{
	SetSize(6);
	SetEmptyMass(CM_DRYMASS + CM_FUEL + SM_DRYMASS);
	SetPMI(_V(2.3, 2.3, 2));

	SetCW(0.5, 0.5, 1, 1);
	SetCrossSections(_V(35, 35, 24));
	SetRotDrag(_V(0.5, 0.5, 0.3));

	SetCOG_elev(2.5);

	SetPitchMomentScale(1e-4);
	SetBankMomentScale(1e-4);

	SetDockParams(_V(0, 0, 4.171), _V(0, 0, 1), _V(0, 1, 0));

	SetThrusters1_Orbit();
	mode = 0;
	CHANGESTATE();
}
void ARTEMIS2::SetConfig1_Orbit(void)
{
	SetSize(6);
	SetEmptyMass(CM_DRYMASS + CM_FUEL + SM_DRYMASS);
	SetPMI(_V(2.3, 2.3, 2));

	SetCW(0.5, 0.5, 1, 1);
	SetCrossSections(_V(35, 35, 24));
	SetRotDrag(_V(0.5, 0.5, 0.3));

	SetCOG_elev(2.5);

	SetPitchMomentScale(1e-4);
	SetBankMomentScale(1e-4);

	SetDockParams(_V(0, 0, 4.171), _V(0, 0, 1), _V(0, 1, 0));

	SetThrusters1_Orbit();
	mode = 1;
	CHANGESTATE();


	//	AddPointLight(_V(0,0,7), 20, 1, 0.5, 0.25,
	//		_COLOUR4(1,1,1,1), COLOUR4(1,1,1,1), COLOUR4(1,1,1,1));

}
void ARTEMIS2::SetConfig3_Drogue(void)
{
	SetSize(25);
	SetEmptyMass(CM_DRYMASS);
	SetTrimScale(0);
	ClearAirfoilDefinitions();

	SetCW(1.5, 1.5, 1, 1);
	SetCrossSections(_V(29, 35, 150));
	SetRotDrag(_V(0.1, 0.1, 0.1));

	SetPitchMomentScale(-2e-4);
	SetBankMomentScale(-2e-4);

	SetPMI(_V(9, 9, 3));

	SetTouchdownPoints(_V(0, -5, -8), _V(-3, -2, -8), _V(3, -2, -8));
	SetSurfaceFrictionCoeff(0.3, 0.3);

	ClearThrusterDefinitions();
	mode = 3;
	CHANGESTATE();
}

void ARTEMIS2::SetConfig4_Chute(void)
{
	SetSize(30);
	SetEmptyMass(CM_DRYMASS);
	SetTrimScale(0);
	ClearAirfoilDefinitions();

	SetCW(2, 2, 1, 1);
	SetCrossSections(_V(26, 26, 2000));
	SetRotDrag(_V(0.5, 0.5, 0.3));

	SetPitchMomentScale(-2e-4);
	SetBankMomentScale(-2e-4);

	SetPMI(_V(9, 9, 3));

	//	SetTouchdownPoints(_V(0,-5,-6.9),_V(-3,-2,-6.9),_V(3,-2,-6.9));
	//	SetSurfaceFrictionCoeff (0.3, 0.3);

	SetTouchdownPoints(tdvtx_geardown, ntdvtx_geardown);

	ClearThrusterDefinitions();
	mode = 4;
	CHANGESTATE();
}

void ARTEMIS2::SetConfig5_Chute_deployed(void)
{
	SetConfig4_Chute();
	mode = 5;
	CHANGESTATE();
}

void ARTEMIS2::SetConfig6_Land(void)
{
	SetSize(12);
	SetEmptyMass(CM_DRYMASS);
	SetPMI(_V(40, 20, 10));
	ClearAirfoilDefinitions();

	SetTouchdownPoints(tdvtx_geardown, ntdvtx_geardown);

	SetThrusters3_Land();
	mode = 6;
	//	balloonMesh = oapiLoadMeshGlobal("Orion-MPCV\\orion-balloons");
	CHANGESTATE();

}
void ARTEMIS2::SetConfig10_Launch0(void)
{
	SetSize(12);
	SetEmptyMass(LAS_DRYMASS + LAS_FUEL + CM_DRYMASS + CM_FUEL + SM_DRYMASS + SM_FUEL + FAIR_MASS + ADAPT_MASS);
	SetPMI(_V(5, 5, 4));

	SetCW(0.5, 0.5, 1, 1);
	SetCrossSections(_V(35, 35, 24));
	SetRotDrag(_V(0.5, 0.5, 0.3));

	SetCOG_elev(2.5);

	SetPitchMomentScale(1e-4);
	SetBankMomentScale(1e-4);

	mode = 10;
	CHANGESTATE();
}

void ARTEMIS2::SetConfig11_Launch1(void)
{
	SetConfig10_Launch0();
	SetEmptyMass(CM_DRYMASS + CM_FUEL + SM_DRYMASS + SM_FUEL + ADAPT_MASS);	// reduce mass of blown fair + LAS
	SetSize(12);
	mode = 11;
	CHANGESTATE();
}
void ARTEMIS2::APPLYSKIN()
{

	oapiSetTexture(dhMesh0, 33, BURNT_tex);
	oapiSetTexture(dhMesh0, 34, BURNT_tex);//hatch

	SetMeshVisibilityMode(19, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield
	SetMeshVisibilityMode(11, MESHVIS_EXTERNAL | MESHVIS_VC);//heatshield
}
void ARTEMIS2::SetConfig12_Abort(void)
{
	SetSize(12);
	SetEmptyMass(LAS_DRYMASS + CM_DRYMASS + CM_FUEL);
	SetPMI(_V(5, 5, 4));

	SetCrossSections(_V(35, 35, 24));
	SetWingAspect(1);
	SetWingEffectiveness(1);

	SetPitchMomentScale(-2e-5);
	SetBankMomentScale(-2e-5);
	SetRotDrag(_V(0.1, 0.1, 0.005));

	SetCameraOffset(_V(0, 0, -1.8));
	SetTouchdownPoints(_V(0, -2, 0), _V(-2, 1, 0), _V(2, 1, 0));
	SetSurfaceFrictionCoeff(0.2, 0.2);

	//	CreateAirfoil (LIFT_VERTICAL,   _V(0, 0.01,0.1), VLiftCoeff, 5.5, 0, 1.27);
	//	CreateAirfoil (LIFT_HORIZONTAL, _V(0, 0,  0.01), HLiftCoeff, 5.5, 0, 1.27);

	ClearAttachments();
	mode = 12;
	CHANGESTATE();
	SetThrusters12_Abort();
}
void ARTEMIS2::SetThrusters1_Orbit()
{
	ClearThrusterDefinitions();
	ClearPropellantResources();
	ph_RM = CreatePropellantResource(SM_FUEL, SM_FUEL, 1);
	SetDefaultPropellantResource(ph_RM);
	THRUSTER_HANDLE th_rcs[50], th_group[4];




	th_rcs[0] = CreateThruster(_V(6, 0, 10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[0], 1.5, .16, _V(.2295357, 2.143019, .06), _V(.7, .7, 0), tex_rcs);
		// AddExhaust(th_rcs[0], 1, 0.1, _V(1.2234, 1.1706, .782), _V(0, 1, 0));
	AddExhaust(th_rcs[0], 1.5, .16, _V(0.2295357, -2.143019, .06), _V(.7, -.7, 0), tex_rcs);
	// Delantero derecho inferior
	th_rcs[1] = CreateThruster(_V(6, 0, 10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[1], 1.5, .16, _V(.2295357, 2.143019, .06), _V(.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[1], 1.5, .16, _V(.2295357, 2.143019, .06), _V(.7, .7, 0), tex_rcs);
	// Delantero derecho proa
	th_rcs[2] = CreateThruster(_V(6, 0, 10), _V(0, 0, -1), RCS_THRUST, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[2], 1.5, 0.16, _V(2.13025, 0.063, -1.440188), _V(.7, 0, .7), tex_rcs);
	AddExhaust(th_rcs[2], 1.5, 0.16, _V(2.13025, -0.057, -1.440188), _V(.7, 0, .7), tex_rcs);
	//AddExhaust(th_rcs[2], .5, .01, _V(1.1555, 1.2106, .766), _V(-0.464, 0, .8858));
	//AddExhaust(th_rcs[2], .5, 0.01, _V(1.148, 1.2179, .782), _V(0, 0, 1));
	// Delantero derecho popa
	th_rcs[3] = CreateThruster(_V(6, 0, 10), _V(0, 0, 1), RCS_THRUST, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[3], 1.5, 0.16, _V(2.13025, 0.063, -1.8), _V(.7, 0, -.7), tex_rcs);
	AddExhaust(th_rcs[3], 1.5, 0.16, _V(2.13025, -0.057, -1.8), _V(.7, 0, -.7), tex_rcs);
	//AddExhaust(th_rcs[3], 1.5, 0.16, _V(2.13025, -0.057,  - 1.853569), _V(.7, 0, -.7), tex_rcs);
	// 1.854029  1.835438
	//	AddExhaust(th_rcs[3], 1, .10, _V(0.922, .046, -7.5872), _V(0, 0, -1));
	// Delantero derecho exterior
	th_rcs[4] = CreateThruster(_V(6, 0, 10), _V(-1, 0, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[4], 1, 0.1, _V(2.1046, 1.0445, 8.739), _V(1, 0, 0));
	//	AddExhaust(th_rcs[4], 1, 0.1, _V(2.1044, 1.0706, 9.2092), _V(1, 0, 0));
	AddExhaust(th_rcs[4], 2.0, 0.16, _V(.2295357, 2.143019, -.068), _V(.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[4], 2.0, 0.16, _V(.2295357, -2.143019, -.068), _V(.7, -.7, 0), tex_rcs);
	// Grupo delantero izquierdo

	// Delantero izquierdo superior
	th_rcs[5] = CreateThruster(_V(-6, 0, 10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[5], 1.5, .16, _V(-.2295357, 2.143019, .06), _V(-.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[5], 1.5, .16, _V(-.2295357, -2.143019, .06), _V(-.7, -.7, 0), tex_rcs);
	//AddExhaust(th_rcs[5], 1, 0.1, _V(-1.2234, 1.1706, .782), _V(0, 1, 0));
	// Delantero izquierdo inferior
	th_rcs[6] = CreateThruster(_V(-6, 0, 10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[6], 1.5, .16, _V(-.2295357, -2.143019, .06), _V(-.7, -.7, 0), tex_rcs);

	AddExhaust(th_rcs[6], 1.5, .16, _V(-.2295357, 2.143019, .06), _V(-.7, .7, 0), tex_rcs);
	// Delantero izquierdo proa
	th_rcs[7] = CreateThruster(_V(-6, 0, 10), _V(0, 0, -1), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[7], 1, .10, _V(-1.7589, .991, 9.6925), _V(0, 0, 1));
	AddExhaust(th_rcs[7], 1.5, 0.16, _V(-2.13025, 0.063, -1.440188), _V(-.7, 0, .7), tex_rcs);
	AddExhaust(th_rcs[7], 1.5, 0.16, _V(-2.13025, -0.057, -1.440188), _V(-.7, 0, .7), tex_rcs);

	//AddExhaust(th_rcs[7], .5, 0.01, _V(-1.148, 1.2179, .782), _V(0, 0, 1));
	// Delantero izquierdo popa
	th_rcs[8] = CreateThruster(_V(-6, 0, 10), _V(0, 0, 1), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[8], 1, .10, _V(-0.922, .046, -7.5872), _V(0, 0, -1));
	AddExhaust(th_rcs[8], 1.5, 0.16, _V(-2.13025, 0.063, -1.8), _V(-.7, 0, -.7), tex_rcs);
	AddExhaust(th_rcs[8], 1.5, 0.16, _V(-2.13025, -0.057, -1.8), _V(-.7, 0, -.7), tex_rcs);
	//AddExhaust(th_rcs[8], 1.5, 0.16, _V(-2.13025, -0.057, -1.853569), _V(-.7, 0, -.7), tex_rcs);

	//AddExhaust(th_rcs[8], .5, 0.01, _V(-1.148, 1.2179, .782), _V(0, 0, -1));
	// Delantero izquierdo exterior
	th_rcs[9] = CreateThruster(_V(-6, 0, 10), _V(1, 0, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[9], 1, 0.1, _V(-2.1046, 1.0445, 8.739), _V(-1, 0, 0));
	//	AddExhaust(th_rcs[9], 1, 0.1, _V(-2.1044, 1.0706, 9.2092), _V(-1, 0, 0));
	AddExhaust(th_rcs[9], 1.5, 0.16, _V(-.2295357, 2.143019, -.068), _V(-.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[9], 1.5, 0.16, _V(-.2295357, -2.143019, -.068), _V(-.7, -.7, 0), tex_rcs);

	// Grupo trasero derecho

	// Trasero derecho superior
	th_rcs[10] = CreateThruster(_V(6, 0, -10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[10], 1, .10, _V(3.4827, .3, -7.4698), _V(0, 1, 0));
	//AddExhaust(th_rcs[10], 1.5, 0.16, _V(.2295357, -2.143019, -.068), _V(.7, -.7, 0), tex_rcs);

	AddExhaust(th_rcs[10], 1.5, 0.16, _V(.2295357, -2.143019, -.068), _V(.7, -.7, 0), tex_rcs);
	//AddExhaust(th_rcs[10], 1, 0.1, _V(1.2234, 1.1706, .782), _V(0, 1, 0));
	// Trasero derecho inferior
	th_rcs[11] = CreateThruster(_V(6, 0, -10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[11], 1.5, 0.16, _V(.2295357, 2.143019, -.068), _V(.7, .7, 0), tex_rcs);
	//AddExhaust(th_rcs[11], 1.5, 0.16, _V(.2295357, -2.143019, -.068), _V(.7, -.7, 0), tex_rcs);
	//	AddExhaust(th_rcs[11], 1, .10, _V(3.2594, -0.092, -7.6734), _V(0, -1, 0));
	// Trasero derecho proa
	th_rcs[12] = CreateThruster(_V(6, 0, -10), _V(0, 0, -1), RCS_THRUST, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[12], 1, .10, _V(1.68, -1.67, -.575), _V(.577, -.577, -.577));
//	AddExhaust(th_rcs[12], 1.5, 0.16, _V(0.074, - 2.133988, - 1.837606), _V(0, -.7,-.7), tex_rcs);
	//AddExhaust(th_rcs[12], 1.5, 0.16, _V( -0.057, -2.13025, -1.440188), _V(0, -.7, .7), tex_rcs);
	AddExhaust(th_rcs[12], 1.5, 0.16, _V(-.044, -2.154, -1.44), _V(0, -.7, .7), tex_rcs);
	AddExhaust(th_rcs[12], 1.5, 0.16, _V(.077, -2.154, -1.44), _V(0, -.7, .7), tex_rcs);
	//AddExhaust(th_rcs[12], .5, 0.01, _V(1.148, 1.2179, -.782), _V(0, 0, 1));
	// Trasero derecho popa
	th_rcs[13] = CreateThruster(_V(6, 0, -10), _V(0, 0, 1), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[13], 1, .10, _V(1.1948, -.120, -9.6909), _V(0, 0, -1));
	AddExhaust(th_rcs[13], 1.5, 0.16, _V(-.044, -2.154, -1.853569), _V(0, -.7, -.7), tex_rcs);
	AddExhaust(th_rcs[13], 1.5, 0.16, _V(.077, -2.154, -1.853569), _V(0, -.7, -.7), tex_rcs);

	//AddExhaust(th_rcs[13], .5, 0.01, _V(1.148, 1.2179, -.782), _V(0, 0, -1));
	// Trasero derecho exterior
	th_rcs[14] = CreateThruster(_V(6, 0, -10), _V(-1, 0, 0), RCS_THRUST, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[14], 1.5, 0.16, _V(.2295357, 2.143019, .06), _V(.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[14], 1.5, 0.16, _V(.2295357, -2.143019, .06), _V(.7, -.7, 0), tex_rcs);
	//	AddExhaust(th_rcs[14], 1, 0.1, _V(3.5573, .195, -7.2458), _V(1, 0, 0));

	// Grupo trasero izquierdo

	// Trasero izquierdo superior
	th_rcs[15] = CreateThruster(_V(-6, 0, -10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[15], 1.5, 0.16, _V(-.2295357, 2.143019, -.068), _V(-.7, .7, 0), tex_rcs);

	AddExhaust(th_rcs[15], 1.5, 0.16, _V(-.2295357, -2.143019, -.068), _V(-.7, -.7, 0), tex_rcs);
	// Trasero izquierdo inferior
	th_rcs[16] = CreateThruster(_V(-6, 0, -10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[16], 1, .10, _V(-3.2594, -0.092, -7.6734), _V(0, -1, 0));
	//AddExhaust(th_rcs[16], 1.5, 0.16, _V(-.2295357, -2.143019, -.068), _V(-.7, -.7, 0), tex_rcs);
	AddExhaust(th_rcs[16], 1.5, 0.16, _V(-.2295357, 2.143019, -.068), _V(-.7, .7, 0), tex_rcs);
	// Trasero izquierdo proa
	th_rcs[17] = CreateThruster(_V(-6, 0, -10), _V(0, 0, -1), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-0.074, - 2.133988, - 1.837606), _V(0, -.7, -.7), tex_rcs);
	//	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-0.057, 2.13025, -1.440188), _V(0, .7,.7), tex_rcs);
	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-.044, 2.154, -1.44), _V(0, .7, .7), tex_rcs);
	AddExhaust(th_rcs[17], 1.5, 0.16, _V(.077, 2.154, -1.44), _V(0, .7, .7), tex_rcs);
	//AddExhaust(th_rcs[17], .5, 0.01, _V(-1.148, 1.2179, -.782), _V(0, 0, 1));
	// Trasero izquierdo popa
	th_rcs[18] = CreateThruster(_V(-6, 0, -10), _V(0, 0, 1), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[18], 1, .10, _V(-1.1948, -.120, -9.6909), _V(0, 0, -1));
	AddExhaust(th_rcs[18], 1.5, 0.16, _V(-.044, 2.154, -1.853569), _V(0, .7, -.7), tex_rcs);
	AddExhaust(th_rcs[18], 1.5, 0.16, _V(.077, 2.154, -1.853569), _V(0, .7, -.7), tex_rcs);

	// Trasero izquierdo exterior
	th_rcs[19] = CreateThruster(_V(-6, 0, -10), _V(1, 0, 0), RCS_THRUST, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[19], 1, 0.1, _V(-3.5573, .195, -6.9846), _V(-1, 0, 0));
	//	AddExhaust(th_rcs[19], 1, 0.1, _V(-3.5573, .195, -7.2458), _V(-1, 0, 0));
	AddExhaust(th_rcs[19], 1.5, 0.16, _V(-.2295357, 2.143019, .06), _V(-.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[19], 1.5, 0.16, _V(-.2295357, -2.143019, .06), _V(-.7, -.7, 0), tex_rcs);
	//pitchrcs
	th_rcs[20] = CreateThruster(_V(6, 0, 10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);//0
	AddExhaust(th_rcs[20], 1.5, 0.16, _V(0.074, 2.14302, -1.87), _V(0, .7, -.7), tex_rcs);

	th_rcs[21] = CreateThruster(_V(6, 0, 10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);//1
	AddExhaust(th_rcs[21], 1.5, 0.16, _V(0.074, 2.14302, -1.432304), _V(0, .7, .7), tex_rcs);

	th_rcs[22] = CreateThruster(_V(-6, 0, 10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);//5
	AddExhaust(th_rcs[22], 1.5, 0.16, _V(-0.074, 2.14302, -1.87), _V(0, .7, -.7), tex_rcs);

	th_rcs[23] = CreateThruster(_V(-6, 0, 10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);//6
	AddExhaust(th_rcs[23], 1.5, 0.16, _V(-0.074, 2.14302, -1.432304), _V(0, .7, .7), tex_rcs);

	th_rcs[24] = CreateThruster(_V(6, 0, -10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);//10
	AddExhaust(th_rcs[24], 1.5, 0.16, _V(0.074, -2.133988, -1.837606), _V(0, -.7, -.7), tex_rcs);

	th_rcs[25] = CreateThruster(_V(6, 0, -10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);//11
	AddExhaust(th_rcs[25], 1.5, 0.16, _V(0.074, -2.14302, -1.432304), _V(0, -.7, .7), tex_rcs);

	th_rcs[26] = CreateThruster(_V(-6, 0, -10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);//15
	AddExhaust(th_rcs[26], 1.5, 0.16, _V(-0.074, -2.133988, -1.837606), _V(0, -.7, -.7), tex_rcs);
	// Trasero izquierdo inferior
	th_rcs[27] = CreateThruster(_V(-6, 0, -10), _V(0, 1, 0), RCS_THRUST, ph_RM, RCS_ISP);//16
	AddExhaust(th_rcs[27], 1.5, 0.16, _V(-0.074, -2.14302, -1.432304), _V(0, -.7, .7), tex_rcs);

	//TRANS
	//OLD 2
	th_rcs[28] = CreateThruster(_V(6, 0, 10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[28], 1.5, 0.16, _V(2.13025, 0.063, -1.440188), _V(.7, 0, .7), tex_rcs);
	AddExhaust(th_rcs[28], 1.5, 0.16, _V(2.13025, -0.057, -1.440188), _V(.7, 0, .7), tex_rcs);

	//OLD 7
	th_rcs[29] = CreateThruster(_V(-6, 0, 10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[7], 1, .10, _V(-1.7589, .991, 9.6925), _V(0, 0, 1));
	AddExhaust(th_rcs[29], 1.5, 0.16, _V(-2.13025, 0.063, -1.440188), _V(-.7, 0, .7), tex_rcs);
	AddExhaust(th_rcs[29], 1.5, 0.16, _V(-2.13025, -0.057, -1.440188), _V(-.7, 0, .7), tex_rcs);
	//OLD 12
	th_rcs[30] = CreateThruster(_V(6, 0, -10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[12], 1, .10, _V(1.68, -1.67, -.575), _V(.577, -.577, -.577));
//	AddExhaust(th_rcs[12], 1.5, 0.16, _V(0.074, - 2.133988, - 1.837606), _V(0, -.7,-.7), tex_rcs);
	//AddExhaust(th_rcs[12], 1.5, 0.16, _V( -0.057, -2.13025, -1.440188), _V(0, -.7, .7), tex_rcs);
	AddExhaust(th_rcs[30], 1.5, 0.16, _V(-.044, -2.154, -1.44), _V(0, -.7, .7), tex_rcs);
	AddExhaust(th_rcs[30], 1.5, 0.16, _V(.077, -2.154, -1.44), _V(0, -.7, .7), tex_rcs);
	//OLD 17
	th_rcs[31] = CreateThruster(_V(-6, 0, -10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-0.074, - 2.133988, - 1.837606), _V(0, -.7, -.7), tex_rcs);
	//	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-0.057, 2.13025, -1.440188), _V(0, .7,.7), tex_rcs);
	AddExhaust(th_rcs[31], 1.5, 0.16, _V(-.044, 2.154, -1.44), _V(0, .7, .7), tex_rcs);
	AddExhaust(th_rcs[31], 1.5, 0.16, _V(.077, 2.154, -1.44), _V(0, .7, .7), tex_rcs);
	//OLD 3
	th_rcs[32] = CreateThruster(_V(6, 0, 10), _V(0, 0, 1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[32], 1.5, 0.16, _V(2.13025, 0.063, -1.8), _V(.7, 0, -.7), tex_rcs);
	AddExhaust(th_rcs[32], 1.5, 0.16, _V(2.13025, -0.057, -1.8), _V(.7, 0, -.7), tex_rcs);
	//OLD8
	th_rcs[33] = CreateThruster(_V(-6, 0, 10), _V(0, 0, 1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[8], 1, .10, _V(-0.922, .046, -7.5872), _V(0, 0, -1));
	AddExhaust(th_rcs[33], 1.5, 0.16, _V(-2.13025, 0.063, -1.8), _V(-.7, 0, -.7), tex_rcs);
	AddExhaust(th_rcs[33], 1.5, 0.16, _V(-2.13025, -0.057, -1.8), _V(-.7, 0, -.7), tex_rcs);
	// 
	//OLD 13
	th_rcs[34] = CreateThruster(_V(6, 0, -10), _V(0, 0, 1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[13], 1, .10, _V(1.1948, -.120, -9.6909), _V(0, 0, -1));
	AddExhaust(th_rcs[34], 1.5, 0.16, _V(-.044, -2.154, -1.853569), _V(0, -.7, -.7), tex_rcs);
	AddExhaust(th_rcs[34], 1.5, 0.16, _V(.077, -2.154, -1.853569), _V(0, -.7, -.7), tex_rcs);

	//OLD18
	th_rcs[35] = CreateThruster(_V(-6, 0, -10), _V(0, 0, 1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[18], 1, .10, _V(-1.1948, -.120, -9.6909), _V(0, 0, -1));
	AddExhaust(th_rcs[35], 1.5, 0.16, _V(-.044, 2.154, -1.853569), _V(0, .7, -.7), tex_rcs);
	AddExhaust(th_rcs[35], 1.5, 0.16, _V(.077, 2.154, -1.853569), _V(0, .7, -.7), tex_rcs);
	// 
	//OLD0
	//th_rcs[36] = CreateThruster(_V(6, 0, 10), _V(0, -1, 0), RCS_THRUST, ph_RM, RCS_ISP);
	th_rcs[36] = CreateThruster(_V(6, 0, 10), _V(0, -1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[36], 1.5, .16, _V(.2295357, 2.143019, .06), _V(.7, .7, 0), tex_rcs);
	// AddExhaust(th_rcs[0], 1, 0.1, _V(1.2234, 1.1706, .782), _V(0, 1, 0));
	// Delantero derecho inferior
	// 	   //OLD5
	th_rcs[37] = CreateThruster(_V(-6, 0, 10), _V(0, -1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[37], 1.5, .16, _V(-.2295357, 2.143019, .06), _V(-.7, .7, 0), tex_rcs);


	//OLD 10
	th_rcs[38] = CreateThruster(_V(6, 0, -10), _V(0, -1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[10], 1, .10, _V(3.4827, .3, -7.4698), _V(0, 1, 0));

	AddExhaust(th_rcs[38], 1.5, 0.16, _V(.2295357, 2.143019, -.068), _V(.7, .7, 0), tex_rcs);
	//AddExhaust(th_rcs[5], 1, 0.1, _V(-1.2234, 1.1706, .782), _V(0, 1, 0));
	// Delantero izquierdo inferior

	//OLD 15
	th_rcs[39] = CreateThruster(_V(-6, 0, -10), _V(0, -1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[39], 1.5, 0.16, _V(-.2295357, 2.143019, -.068), _V(-.7, .7, 0), tex_rcs);


	//OLD 6
	th_rcs[41] = CreateThruster(_V(-6, 0, 10), _V(0, 1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[41], 1.5, .16, _V(-.2295357, -2.143019, .06), _V(-.7, -.7, 0), tex_rcs);
	//OLD1
	th_rcs[40] = CreateThruster(_V(6, 0, 10), _V(0, 1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[40], 1.5, .16, _V(.2295357, -2.143019, .06), _V(.7, -.7, 0), tex_rcs);
	//OLD 11
	th_rcs[42] = CreateThruster(_V(6, 0, -10), _V(0, 1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);

	AddExhaust(th_rcs[42], 1.5, 0.16, _V(.2295357, -2.143019, -.068), _V(.7, -.7, 0), tex_rcs);

	//	OLD16
	th_rcs[43] = CreateThruster(_V(-6, 0, -10), _V(0, 1, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[16], 1, .10, _V(-3.2594, -0.092, -7.6734), _V(0, -1, 0));
	AddExhaust(th_rcs[43], 1.5, 0.16, _V(-.2295357, -2.143019, -.068), _V(-.7, -.7, 0), tex_rcs);

	//OLD 9
	th_rcs[44] = CreateThruster(_V(-6, 0, 10), _V(1, 0, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[9], 1, 0.1, _V(-2.1046, 1.0445, 8.739), _V(-1, 0, 0));
	//	AddExhaust(th_rcs[9], 1, 0.1, _V(-2.1044, 1.0706, 9.2092), _V(-1, 0, 0));
	AddExhaust(th_rcs[44], 1.5, 0.16, _V(-.2295357, 2.143019, -.068), _V(-.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[44], 1.5, 0.16, _V(-.2295357, -2.143019, -.068), _V(-.7, -.7, 0), tex_rcs);
	//OLD 19
	th_rcs[45] = CreateThruster(_V(-6, 0, -10), _V(1, 0, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[19], 1, 0.1, _V(-3.5573, .195, -6.9846), _V(-1, 0, 0));
	//	AddExhaust(th_rcs[19], 1, 0.1, _V(-3.5573, .195, -7.2458), _V(-1, 0, 0));
	AddExhaust(th_rcs[45], 1.5, 0.16, _V(-.2295357, 2.143019, .06), _V(-.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[45], 1.5, 0.16, _V(-.2295357, -2.143019, .06), _V(-.7, -.7, 0), tex_rcs);
	//OLD 4
	th_rcs[46] = CreateThruster(_V(6, 0, 10), _V(-1, 0, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[4], 1, 0.1, _V(2.1046, 1.0445, 8.739), _V(1, 0, 0));
	//	AddExhaust(th_rcs[4], 1, 0.1, _V(2.1044, 1.0706, 9.2092), _V(1, 0, 0));
	AddExhaust(th_rcs[46], 2.0, 0.16, _V(.2295357, 2.143019, -.068), _V(.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[46], 2.0, 0.16, _V(.2295357, -2.143019, -.068), _V(.7, -.7, 0), tex_rcs);
	//OLD 14
	th_rcs[47] = CreateThruster(_V(6, 0, -10), _V(-1, 0, 0), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	AddExhaust(th_rcs[47], 1.5, 0.16, _V(.2295357, 2.143019, .06), _V(.7, .7, 0), tex_rcs);
	AddExhaust(th_rcs[47], 1.5, 0.16, _V(.2295357, -2.143019, .06), _V(.7, -.7, 0), tex_rcs);
	////visual rcs


	// Grupo motor de cabeceo hacia arriba "PITCH-UP"1,6,10,15
	th_group[0] = th_rcs[21];
	th_group[1] = th_rcs[23];
	th_group[2] = th_rcs[24];
	th_group[3] = th_rcs[26];
	//th_group[0] = th_rcs[3];
	//th_group[1] = th_rcs[8];
	//th_group[2] = th_rcs[12];
	//th_group[3] = th_rcs[17];
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_PITCHUP);

	// Grupo motor de cabeceo hacia abajo "PITCH-DOWN" 0,5,11,16
	th_group[0] = th_rcs[20];

	th_group[1] = th_rcs[22];
	th_group[2] = th_rcs[25];
	th_group[3] = th_rcs[27];
	//th_group[4] = th_rcs[21];
	//th_group[0] = th_rcs[2];
	//th_group[1] = th_rcs[7];
	//th_group[2] = th_rcs[13];
	//th_group[3] = th_rcs[18];
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_PITCHDOWN);

	// Grupo motor de balanceo izquierda "YAW-RIGHT"
	th_group[0] = th_rcs[8];
	th_group[1] = th_rcs[18];
	th_group[2] = th_rcs[2];
	th_group[3] = th_rcs[12];
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_YAWRIGHT);

	// Grupo motor de balanceo derecha "YAW-LEFT"
	th_group[0] = th_rcs[3];
	th_group[1] = th_rcs[13];
	th_group[2] = th_rcs[7];
	th_group[3] = th_rcs[17];
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_YAWLEFT);

	// Grupo motor de alabeo derecho "BANK-RIGHT"
	th_group[0] = th_rcs[0];
	th_group[1] = th_rcs[6];
	th_group[2] = th_rcs[10];
	th_group[3] = th_rcs[16];
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_BANKRIGHT);

	// Grupo motor de alabeo izquierdo "BANK-LEFT"
	th_group[0] = th_rcs[1];
	th_group[1] = th_rcs[5];
	th_group[2] = th_rcs[11];
	th_group[3] = th_rcs[15];
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_BANKLEFT);

	// 2-Movimientos traslacionales

	// Grupo motor de traslación derecha "RIGHT"
	// Este grupo lo podían formar los impulsores izquierdos delantero de proa y trasero de popa
	th_group[0] = th_rcs[44];//9
	th_group[1] = th_rcs[45];//19
	CreateThrusterGroup(th_group, 2, THGROUP_ATT_RIGHT);

	// Grupo motor de traslación izquierda "LEFT"
	// Este grupo lo podían formar los impulsores derechos delantero de proa y trasero de popa
	th_group[0] = th_rcs[46];//4
	th_group[1] = th_rcs[47];//14
	CreateThrusterGroup(th_group, 2, THGROUP_ATT_LEFT);

	// Grupo motor de traslación arriba "DOWN"
	th_group[0] = th_rcs[36];//0
	th_group[1] = th_rcs[37];//5
	th_group[2] = th_rcs[38];//10
	th_group[3] = th_rcs[39];//15

	//th_group[0] = th_rcs[0];
	//th_group[1] = th_rcs[5];
	//th_group[2] = th_rcs[10];
	//th_group[3] = th_rcs[15];



	CreateThrusterGroup(th_group, 4, THGROUP_ATT_DOWN);

	// Grupo motor de traslación abajo "UP"
	th_group[0] = th_rcs[40];//1
	th_group[1] = th_rcs[41];//6
	th_group[2] = th_rcs[42];//11
	th_group[3] = th_rcs[43];//16
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_UP);

	// Grupo motor de traslación adelante "FORWARD"
	th_group[0] = th_rcs[32];//3
	th_group[1] = th_rcs[33];//8
	th_group[2] = th_rcs[34];//13
	th_group[3] = th_rcs[35];//18
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_FORWARD);

	// Grupo motor de traslación atrás "BACK"
	th_group[0] = th_rcs[28];//2
	th_group[1] = th_rcs[29];//7
	th_group[2] = th_rcs[30];//12
	th_group[3] = th_rcs[31];//17
	CreateThrusterGroup(th_group, 4, THGROUP_ATT_BACK);
	THRUSTER_HANDLE  th_att_lin[2];

	/*

	THRUSTER_HANDLE th_att_rot[16], th_att_lin[16];
	// Pitchup
	th_att_rot[0] = th_att_lin[8] = CreateThruster(_V(0.06, 2.10, -1.49), _V(0, -0.7, -0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[1] = th_att_lin[9] = CreateThruster(_V(-0.06, 2.10, -1.49), _V(0, -0.7, -0.7), RCS_THRUST, ph_RM, RCS_ISP);

	th_att_rot[2] = th_att_lin[0] = CreateThruster(_V(0.06, -2.10, .582), _V(0, 0.7, 0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[3] = th_att_lin[1] = CreateThruster(_V(-0.06, -2.10, -1.82), _V(0, 0.7, 0.7), RCS_THRUST, ph_RM, RCS_ISP);

	// Pitchdown
	th_att_rot[4] = th_att_lin[2] = CreateThruster(_V(0.06, 2.10, -1.82), _V(0, -0.7, 0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[5] = th_att_lin[3] = CreateThruster(_V(-0.06, 2.10, -1.82), _V(0, -0.7, 0.7), RCS_THRUST, ph_RM, RCS_ISP);

	th_att_rot[6] = th_att_lin[10] = CreateThruster(_V(0.06, -2.10, -1.49), _V(0, 0.7, -0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[7] = th_att_lin[11] = CreateThruster(_V(-0.06, -2.10, -1.49), _V(0, 0.7, -0.7), RCS_THRUST, ph_RM, RCS_ISP);

	// Yawleft
	th_att_rot[8] = th_att_lin[4] = CreateThruster(_V(2.10, 0.06, -1.82), _V(-0.7, 0, 0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[9] = th_att_lin[5] = CreateThruster(_V(2.10, -0.06, -1.82), _V(-0.7, 0, 0.7), RCS_THRUST, ph_RM, RCS_ISP);

	th_att_rot[10] = th_att_lin[12] = CreateThruster(_V(-2.10, 0.06, -1.49), _V(0.7, 0, -0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[11] = th_att_lin[13] = CreateThruster(_V(-2.10, -0.06, -1.49), _V(0.7, 0, -0.7), RCS_THRUST, ph_RM, RCS_ISP);

	// Yawright
	th_att_rot[12] = th_att_lin[14] = CreateThruster(_V(2.10, 0.06, -1.49), _V(-0.7, 0, -0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[13] = th_att_lin[15] = CreateThruster(_V(2.10, -0.06, -1.49), _V(-0.7, 0, -0.7), RCS_THRUST, ph_RM, RCS_ISP);

	th_att_rot[14] = th_att_lin[6] = CreateThruster(_V(-2.10, 0.06, -1.82), _V(0.7, 0, 0.7), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[15] = th_att_lin[7] = CreateThruster(_V(-2.10, -0.06, -1.82), _V(0.7, 0, 0.7), RCS_THRUST, ph_RM, RCS_ISP);

	CreateThrusterGroup(th_att_rot, 4, THGROUP_ATT_PITCHUP);
	CreateThrusterGroup(th_att_rot + 4, 4, THGROUP_ATT_PITCHDOWN);

	CreateThrusterGroup(th_att_rot + 8, 4, THGROUP_ATT_YAWLEFT);
	CreateThrusterGroup(th_att_rot + 12, 4, THGROUP_ATT_YAWRIGHT);

	CreateThrusterGroup(th_att_lin, 8, THGROUP_ATT_FORWARD);
	CreateThrusterGroup(th_att_lin + 8, 8, THGROUP_ATT_BACK);

	for (int i = 0; i<16; i++) {
	AddExhaust(th_att_rot[i], R_SIZE, tex_rcs);
	AddExhaustStream(th_att_rot[i], &rcs_stream);
	}

	// Bankleft
	th_att_rot[0] = th_att_lin[0] = th_att_lin[12] = CreateThruster(_V(-0.18, -2.10, 0.06), _V(0.7, 0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[1] = th_att_lin[1] = th_att_lin[13] = CreateThruster(_V(-0.18, -2.10, -0.06), _V(0.7, 0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);

	th_att_rot[2] = th_att_lin[4] = th_att_lin[8] = CreateThruster(_V(0.18, 2.10, 0.06), _V(-0.7, -0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[3] = th_att_lin[5] = th_att_lin[9] = CreateThruster(_V(0.18, 2.10, -0.06), _V(-0.7, -0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);

	// Bankright
	th_att_rot[4] = th_att_lin[2] = th_att_lin[10] = CreateThruster(_V(0.18, -2.10, 0.06), _V(-0.7, 0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[5] = th_att_lin[3] = th_att_lin[11] = CreateThruster(_V(0.18, -2.10, -0.06), _V(-0.7, 0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);

	th_att_rot[6] = th_att_lin[6] = th_att_lin[14] = CreateThruster(_V(-0.18, 2.10, 0.06), _V(0.7, -0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);
	th_att_rot[7] = th_att_lin[7] = th_att_lin[15] = CreateThruster(_V(-0.18, 2.10, -0.06), _V(0.7, -0.7, 0), RCS_THRUST, ph_RM, RCS_ISP);

	CreateThrusterGroup(th_att_rot, 4, THGROUP_ATT_BANKLEFT);
	CreateThrusterGroup(th_att_rot + 4, 4, THGROUP_ATT_BANKRIGHT);

	CreateThrusterGroup(th_att_lin, 4, THGROUP_ATT_UP);
	CreateThrusterGroup(th_att_lin + 4, 4, THGROUP_ATT_DOWN);
	CreateThrusterGroup(th_att_lin + 8, 4, THGROUP_ATT_LEFT);
	CreateThrusterGroup(th_att_lin + 12, 4, THGROUP_ATT_RIGHT);

	int i;
	for (i = 0; i<8; i++) {
	AddExhaust(th_att_rot[i], R_SIZE, tex_rcs);
	AddExhaustStream(th_att_rot[i], &rcs_stream);
	}
	*/
	// main
	th_att_lin[0] = CreateThruster(_V(0, 0, -2.7), _V(0, 0, 1), MAIN_THRUST, ph_RM, MAIN_ISP);
	CreateThrusterGroup(th_att_lin, 1, THGROUP_MAIN);
	AddExhaust(th_att_lin[0], MAIN_ENGSIZE, tex_main);

	// main dummy for exhaust stream
	th_att_lin[1] = CreateThruster(_V(0, 0, -2.7), _V(0, 0, 1), 0, ph_RM, MAIN_ISP);
	CreateThrusterGroup(th_att_lin, 2, THGROUP_MAIN);
	AddExhaustStream(th_att_lin[1], &main_stream);


	ph_descent = CreatePropellantResource(LEM_DescentFUEL);	// Descent stage propellant tank
	// Define Thrusters: Main Engines 
	SetDefaultPropellantResource(ph_descent);// Set descent tanks as default propellant resource
	th_descent[0] = CreateThruster(_V(0.0, 0, 0), _V(0, 1, 0), LEM_DescentTHRUST, ph_descent, LEM_DescentISP); // Orbiral stage engine
	SetThrusterResource(th_descent[0], ph_descent);

	th_group[0] = th_descent[0];

	CreateThrusterGroup(th_group, 1, THGROUP_HOVER);

	AddExhaust(th_descent[0], MAIN_ENGSIZE, tex_main);






	// aux
	th_aux[0] = CreateThruster(_V(1.3, 0.16, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[1] = CreateThruster(_V(1.3, -0.16, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[2] = CreateThruster(_V(-1.3, 0.16, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[3] = CreateThruster(_V(-1.3, -0.16, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[4] = CreateThruster(_V(0.16, 1.3, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[5] = CreateThruster(_V(-0.16, 1.3, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[6] = CreateThruster(_V(0.16, -1.3, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);
	th_aux[7] = CreateThruster(_V(-0.16, -1.3, -2.4), _V(0, 0, 1), AUX_THRUST, ph_RM, RCS_ISP);

	CreateThrusterGroup(th_aux, 1, THGROUP_USER);


	AddExhaust(th_aux[0], 2.5, 0.4, _V(1.058, 0.8, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[1], 2.5, 0.4, _V(.822, 1.045, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[2], 2.5, 0.4, _V(-.802, 1.035, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[3], 2.5, 0.4, _V(-1.008, .809, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[4], 2.5, 0.4, _V(-1.018058, -.8046011, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[5], 2.5, 0.4, _V(-.7916822, -1.030977, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[6], 2.5, 0.4, _V(.8224722, -1.021134, -2.435), _V(0, 0, -1), tex_rcs);
	AddExhaust(th_aux[7], 2.5, 0.4, _V(1.05869, -.8046011, -2.435), _V(0, 0, -1), tex_rcs);













	//for (i = 0; i<8; i++) {
	//AddExhaust(th_aux[i], AUX_SIZE, tex_rcs);
	//AddExhaustStream(th_aux[i], &rcs_stream);
}

void ARTEMIS2::SetThrusters2_Reentry()
{
	ClearThrusterDefinitions();
	ClearPropellantResources();

	ph_CM = CreatePropellantResource(CM_FUEL);	 // CM propellant
	SetDefaultPropellantResource(ph_CM);

	THRUSTER_HANDLE th_att_rot[16], th_att_lin[16];
	// Pitchup
	th_att_rot[0] = th_att_lin[2] = CreateThruster(_V(0.238, 1.9964, 2.2087), _V(0, -0.1, -1), CRCS_THRUST, ph_CM, RCS_ISP);
	th_att_rot[1] = th_att_lin[3] = CreateThruster(_V(-0.238, 1.9964, 2.2087), _V(0, -0.1, -1), CRCS_THRUST, ph_CM, RCS_ISP);

	// Pitchdown	
	th_att_rot[2] = th_att_lin[0] = CreateThruster(_V(0.330, 1.2874, 3.3965), _V(0, -1, -0.1), CRCS_THRUST, ph_CM, RCS_ISP);
	th_att_rot[3] = th_att_lin[1] = CreateThruster(_V(-0.330, 1.2874, 3.3965), _V(0, -1, -0.1), CRCS_THRUST, ph_CM, RCS_ISP);

	// Yawleft
	th_att_rot[4] = th_att_lin[4] = CreateThruster(_V(-2.0639, 0.085, 2.1347), _V(-0.1, 0, -1), CRCS_THRUST, ph_CM, RCS_ISP);
	th_att_rot[5] = th_att_lin[5] = CreateThruster(_V(-2.0639, -0.085, 2.1347), _V(-0.1, 0, -1), CRCS_THRUST, ph_CM, RCS_ISP);

	// Yawright
	th_att_rot[6] = th_att_lin[6] = CreateThruster(_V(2.0639, 0.085, 2.1347), _V(0.1, 0, -1), CRCS_THRUST, ph_CM, RCS_ISP);
	th_att_rot[7] = th_att_lin[7] = CreateThruster(_V(2.0639, -0.085, 2.1347), _V(0.1, 0, -1), CRCS_THRUST, ph_CM, RCS_ISP);

	CreateThrusterGroup(th_att_rot, 2, THGROUP_ATT_PITCHUP);
	CreateThrusterGroup(th_att_rot + 2, 2, THGROUP_ATT_PITCHDOWN);
	CreateThrusterGroup(th_att_rot + 4, 2, THGROUP_ATT_YAWLEFT);
	CreateThrusterGroup(th_att_rot + 6, 2, THGROUP_ATT_YAWRIGHT);

	CreateThrusterGroup(th_att_lin + 4, 4, THGROUP_ATT_BACK);

	for (int i = 0; i < 8; i++) {
		AddExhaust(th_att_rot[i], FR_SIZE, tex_rcs);
		AddExhaustStream(th_att_rot[i], &rcs_stream);
	}

	// Bankleft
	th_att_rot[0] = th_att_lin[2] = CreateThruster(_V(-1.4904, 1.5364, 2.0066), _V(-0.2, -0.8, 0), 3 * CRCS_THRUST, ph_CM, RCS_ISP);
	th_att_rot[1] = th_att_lin[3] = CreateThruster(_V(-.992, 1.9004, 1.9832), _V(-0.2, -0.8, 0), 3 * CRCS_THRUST, ph_CM, RCS_ISP);

	// Bankright
	th_att_rot[2] = th_att_lin[4] = CreateThruster(_V(1.4904, 1.5364, 2.0066), _V(0.2, -0.8, 0), 3 * CRCS_THRUST, ph_CM, RCS_ISP);
	th_att_rot[3] = th_att_lin[5] = CreateThruster(_V(.992, 1.9004, 1.9832), _V(0.2, -0.8, 0), 3 * CRCS_THRUST, ph_CM, RCS_ISP);

	CreateThrusterGroup(th_att_rot, 2, THGROUP_ATT_BANKLEFT);
	CreateThrusterGroup(th_att_rot + 2, 2, THGROUP_ATT_BANKRIGHT);

	CreateThrusterGroup(th_att_lin, 6, THGROUP_ATT_DOWN);

	//	int i;
	for (int i = 0; i < 4; i++) {
		AddExhaust(th_att_rot[i], FR_SIZE, tex_rcs);
		AddExhaustStream(th_att_rot[i], &rcs_stream);
	}
}
void ARTEMIS2::SetThrusters3_Land()
{
	ph_CM = CreatePropellantResource(10);	 // ground braking propellant

	THRUSTER_HANDLE th_att_lin[16];

	th_att_lin[0] = CreateThruster(_V(2, 0, -5), _V(-1, 0, 1), 40000, ph_CM, RCS_ISP);
	th_att_lin[1] = CreateThruster(_V(-2, 0, -5), _V(1, 0, 1), 40000, ph_CM, RCS_ISP);
	th_att_lin[2] = CreateThruster(_V(0, 2, -5), _V(0, -1, 1), 40000, ph_CM, RCS_ISP);
	th_att_lin[3] = CreateThruster(_V(0, -2, -5), _V(0, 1, 1), 40000, ph_CM, RCS_ISP);

	CreateThrusterGroup(th_att_lin, 4, THGROUP_ATT_UP);
	SetThrusterGroupLevel(THGROUP_ATT_UP, 1);

	for (int i = 0; i < 4; i++)
		AddExhaustStream(th_att_lin[i], &land_stream);
}
void ARTEMIS2::SetThrusters12_Abort()
{
	ClearThrusterDefinitions();
	ClearPropellantResources();
	ph_main = CreatePropellantResource(LAS_FUEL);	 // LAS propellant
	SetDefaultPropellantResource(ph_main);
	ph_RM = CreatePropellantResource(0.0002); // Smoke fuel

// Abort thrusters, starting with +x
	THRUSTER_HANDLE th_att_lin[12];
	th_att_lin[0] = CreateThruster(_V(0.7, 0, 8.2), _V(-0.5, 0, 0.7), 563250, ph_main, LAS_ISP);
	th_att_lin[1] = CreateThruster(_V(-0.7, 0, 8.2), _V(0.5, 0, 0.7), 563250, ph_main, LAS_ISP);
	th_att_lin[2] = CreateThruster(_V(0, 0.7, 8.2), _V(0, -0.5, 0.7), 563260, ph_main, LAS_ISP);
	th_att_lin[3] = CreateThruster(_V(0, -0.7, 8.2), _V(0, 0.5, 0.7), 1.0001 * 563250, ph_main, LAS_ISP);
	th_att_lin[4] = CreateThruster(_V(0, 0, 4.5), _V(0, 0, 1), 0, ph_main, LAS_ISP);

	th_att_lin[5] = CreateThruster(_V(0.4, 0, 13.7), _V(-1, 0, 0.2), 0, ph_main, LAS_ISP);
	th_att_lin[6] = CreateThruster(_V(-0.4, 0, 13.7), _V(1, 0, 0.2), 0, ph_main, LAS_ISP);
	th_att_lin[7] = CreateThruster(_V(0, 0.4, 13.7), _V(0, -1, 0.2), 0, ph_main, LAS_ISP);
	th_att_lin[8] = CreateThruster(_V(0, -0.4, 13.7), _V(0, 1, 0.2), 0, ph_main, LAS_ISP);

	th_att_lin[9] = CreateThruster(_V(0, 0, 7), _V(0, 0, 1), 0.1, ph_RM, LAS_ISP);

	CreateThrusterGroup(th_att_lin, 9, THGROUP_MAIN);
	for (int i = 0; i < 4; i++) {
		AddExhaust(th_att_lin[i], 5, 1);
		SetThrusterLevel(th_att_lin[i], 1.0);
	}
	AddExhaustStream(th_att_lin[4], &srb_stream);
	SetThrusterLevel(th_att_lin[4], 1);

	for (int i = 5; i < 9; i++) {
		AddExhaust(th_att_lin[i], AUX_SIZE, tex_rcs);
		AddExhaustStream(th_att_lin[i], &rcs_stream);
		SetThrusterLevel(th_att_lin[i], 1.0);
	}
	AddExhaustStream(th_att_lin[9], &srb_stream);
	SetThrusterLevel(th_att_lin[9], 0.51);
}
void ARTEMIS2::SetConfig2_Reentry(void)
{
	SetSize(6);
	SetEmptyMass(CM_DRYMASS);
	SetPMI(_V(1, 1, 1));

	//	SetCW (0.5, 0.5, 1, 1);
	SetCrossSections(_V(24, 28, 19));
	SetWingAspect(1);
	SetWingEffectiveness(1);

	SetPitchMomentScale(-1e-5);
	SetBankMomentScale(-1e-5);
	SetRotDrag(_V(0.01, 0.01, 0.005));

	SetCameraOffset(_V(0, 0, -1.8));
	SetTouchdownPoints(_V(0, -2, 0), _V(-2, 1, 0), _V(2, 1, 0));
	SetSurfaceFrictionCoeff(0.2, 0.2);

	//SetDockParams(_V(0, 0, 2.1), _V(0, 0, 1), _V(0, 1, 0));

	CreateAirfoil(LIFT_VERTICAL, _V(0, 0.01, 0.1), VLiftCoeff, 5.5, 0, 1.27);
	CreateAirfoil(LIFT_HORIZONTAL, _V(0, 0, 0.01), HLiftCoeff, 5.5, 0, 1.27);

	mode = 2;
	CHANGESTATE();
	SetThrusters2_Reentry();
}
void ARTEMIS2::blowFairing() {
	if (BLOWNFAIRING == 0) {
		PROPELLANT_HANDLE ph_lhf_aux, ph_rhf_aux;
		THRUSTER_HANDLE th_lhf_aux, th_rhf_aux;

		VESSELSTATUS vs;
		char name[256] = { "" };
		//	THRUSTER_HANDLE th_att_lin[16];
		//	OBJHANDLE h;
		VESSEL* v;

		//	ReloadMeshes();
		CHANGESTATE();
		SetMeshVisibilityMode(16, MESHVIS_NEVER);
		SetMeshVisibilityMode(17, MESHVIS_NEVER);
		SetMeshVisibilityMode(18, MESHVIS_NEVER);

		GetStatus(vs);
		vs.flag[0] = 3;
		ofs = _V(0, 0, 0);
		Local2Rel(ofs, vs.rpos);

		sprintf_s(name, "%s-fair1", GetName());
		Local2Rel(ofs, vs.rpos);
		fair1h = oapiCreateVessel(name, "ARTEMIS_FAIR1", vs);
		v = oapiGetVesselInterface(fair1h);

		v->SetEmptyMass(200);
		ph_lhf_aux = v->CreatePropellantResource(5, 5, 1.0);
		th_lhf_aux = v->CreateThruster(_V(0, 0.0, 1.0), _V(0, -0.95, 0.1), 5000.0, ph_lhf_aux, 300.0, 0.0, 0);
		v->SetThrusterLevel(th_lhf_aux, 1.0);
		v->SetEnableFocus(false);
		//******
		GetStatus(vs);
		vs.flag[0] = 3;
		ofs = _V(0, 0, 0);
		Local2Rel(ofs, vs.rpos);

		sprintf_s(name, "%s-fair2", GetName());
		Local2Rel(ofs, vs.rpos);
		//		vs.arot.z += PI;
		fair2h = oapiCreateVessel(name, "ARTEMIS_FAIR2", vs);
		v = oapiGetVesselInterface(fair2h);

		v->SetEmptyMass(200);
		ph_rhf_aux = v->CreatePropellantResource(5, 5, 1.0);
		th_rhf_aux = v->CreateThruster(_V(0, 0.0, 1.0), _V(-0.87, 0.5, 0.1), 5000.0, ph_rhf_aux, 300.0, 0.0, 0);
		v->SetThrusterLevel(th_rhf_aux, 1.0);
		v->SetEnableFocus(false);

		//******
		GetStatus(vs);
		vs.flag[0] = 3;
		ofs = _V(0, 0, 0);
		Local2Rel(ofs, vs.rpos);

		sprintf_s(name, "%s-fair3", GetName());
		Local2Rel(ofs, vs.rpos);
		//		vs.arot.z += PI;
		fair3h = oapiCreateVessel(name, "ARTEMIS_FAIR3", vs);
		v = oapiGetVesselInterface(fair3h);

		v->SetEmptyMass(200);
		ph_rhf_aux = v->CreatePropellantResource(5, 5, 1.0);
		th_rhf_aux = v->CreateThruster(_V(0, 0.0, 1.0), _V(0.87, 0.5, 0.1), 5000.0, ph_rhf_aux, 300.0, 0.0, 0);
		v->SetThrusterLevel(th_rhf_aux, 1.0);
		v->SetEnableFocus(false);
		BLOWNFAIRING = 1;
	}
}


void ARTEMIS2::blowCover() {
	SetMeshVisibilityMode(3, MESHVIS_NEVER);
	// Create the upper cover as an individual object
	VESSELSTATUS vs;
	GetStatus(vs);
	vs.flag[0] = 3;
	VECTOR3 ofs = { 0, 0, 0 };
	Local2Rel(ofs, vs.rpos);

	VECTOR3 vel = _V(0, 0.5, -1);
	VECTOR3 rofs, rvel = { vs.rvel.x, vs.rvel.y, vs.rvel.z };
	Local2Rel(ofs, vs.rpos);
	GlobalRot(vel, rofs);
	vs.rvel.x = rvel.x + rofs.x;
	vs.rvel.y = rvel.y + rofs.y;
	vs.rvel.z = rvel.z + rofs.z;

	char name[256] = { "" };
	strcpy(name, GetName()); strcpy(name, "-cover");
	oapiCreateVessel(name, "ARTEMISDOCKCOVER", vs);

	if (oapiGetTimeAcceleration() > 10)
		oapiSetTimeAcceleration(10);
}

void ARTEMIS2::blowDrogue() {		// Create the drogue as an individual object
	VESSELSTATUS vs;
	GetStatus(vs);
	vs.flag[0] = 3;
	VECTOR3 ofs = { 0, 0, -2 };
	Local2Rel(ofs, vs.rpos);

	VECTOR3 vel = _V(0, 0, -2);
	VECTOR3 rofs, rvel = { vs.rvel.x, vs.rvel.y, vs.rvel.z };
	Local2Rel(ofs, vs.rpos);
	GlobalRot(vel, rofs);
	vs.rvel.x = rvel.x + rofs.x;
	vs.rvel.y = rvel.y + rofs.y;
	vs.rvel.z = rvel.z + rofs.z;

	char name[256];
	strcpy(name, GetName()); strcpy(name, "-Drogue");
	oapiCreateVessel(name, "ARTEMIS_DROGUE", vs);
}

void ARTEMIS2::blowChute() {		// Create the chute as an individual object
	oapiSetTimeAcceleration(1);

	VESSELSTATUS vs;
	GetStatus(vs);
	vs.flag[0] = 3;
	VECTOR3 ofs = { 0, 0, 2 };
	Local2Rel(ofs, vs.rpos);

	VECTOR3 vel = _V(0, -30, 3);
	VECTOR3 rofs, rvel = { vs.rvel.x, vs.rvel.y, vs.rvel.z };
	Local2Rel(ofs, vs.rpos);
	GlobalRot(vel, rofs);
	vs.rvel.x = rvel.x + rofs.x;
	vs.rvel.y = rvel.y + rofs.y;
	vs.rvel.z = rvel.z + rofs.z;

	char name[256];
	strcpy(name, GetName()); strcpy(name, "-Chute");
	oapiCreateVessel(name, "ARTEMIS_CHUTE", vs);
}

void ARTEMIS2::blowSM() {
	int j = 0;

	if (oapiGetTimeAcceleration() > 10)		// Slow down
		oapiSetTimeAcceleration(10);

	if (mode > 9) j++;
	SetConfig2_Reentry();

	VESSELSTATUS vs;
	char name[256] = { "" };
	THRUSTER_HANDLE th_att_lin[16];
	OBJHANDLE h;


	GetStatus(vs);
	vs.flag[0] = 3;
	VECTOR3 ofs = { 0,0,-2.06 };
	Local2Rel(ofs, vs.rpos);

	VECTOR3 vel = _V(0, 0, -0.02);
	VECTOR3 rofs, rvel = { vs.rvel.x, vs.rvel.y, vs.rvel.z };
	Local2Rel(ofs, vs.rpos);
	GlobalRot(vel, rofs);
	vs.rvel.x = rvel.x + rofs.x;
	vs.rvel.y = rvel.y + rofs.y;
	vs.rvel.z = rvel.z + rofs.z;

	strcpy(name, GetName()); strcat(name, "-SM");
	h = oapiCreateVessel(name, "ARTEMISSM", vs);
	//v = oapiGetVesselInterface(h);
	VESSEL3* v = (VESSEL3*)oapiGetVesselInterface(h);
	char abuf[256];
	sprintf(abuf, "%0.4f", SPANELFOLD_proc);
	v->clbkConsumeBufferedKey(COMM_SEND_SLPD, false, abuf);

	sprintf(abuf, "%0.4f", WING1ROTATE_proc);
	v->clbkConsumeBufferedKey(COMM_SEND_SLP1, false, abuf);

	sprintf(abuf, "%0.4f", WING2ROTATE_proc);
	v->clbkConsumeBufferedKey(COMM_SEND_SLP2, false, abuf);

	sprintf(abuf, "%0.4f", WING3ROTATE_proc);
	v->clbkConsumeBufferedKey(COMM_SEND_SLP3, false, abuf);

	sprintf(abuf, "%0.4f", WING4ROTATE_proc);
	v->clbkConsumeBufferedKey(COMM_SEND_SLP4, false, abuf);


	//	v->SetAnimation(anim_SPANELFOLD, SPANELFOLD_proc);
		//SetAnimation(anim_HATCH, HATCH_proc);
	//	v->SetAnimation(anim_SPANEL1ROTATE, WING1ROTATE_proc);
	//	v->SetAnimation(anim_SPANEL2ROTATE, WING2ROTATE_proc);
	//	v->SetAnimation(anim_SPANEL3ROTATE, WING3ROTATE_proc);
	//	v->SetAnimation(anim_SPANEL4ROTATE, WING4ROTATE_proc);





		//		v->SetAnimation(anim_JOINT2, JOINT2_proc);
		//		v->SetAnimation(anim_JOINT1, JOINT1_proc);
		//		v->SetAnimation(anim_JOINT7, ARM3_proc);
		//		v->SetAnimation(anim_HGA, HGA_proc);



	v->SetEmptyMass(SM_DRYMASS);
	ph_RM = v->CreatePropellantResource(10.0, 10.0, 1.0);
	//  CreateThruster (_V( 0.06, 2.10, -1.49), _V(0,-0.7,-0.7), RCS_THRUST, ph_RM, RCS_ISP);
/*
		th_att_lin[8] = v->CreateThruster(_V(6, 0, 10), _V(0, 0, -1), RCS_THRUST / 3, ph_RM, RCS_ISP);
		v->AddExhaust(th_att_lin[8], 1.5, 0.16, _V(1.854029, 1.835438, .984), _V(0, 0, 1), tex_rcs);
		th_att_lin[9] = v->CreateThruster(_V(-6, 0, 10), _V(0, 0, -1), RCS_THRUST / 3, ph_RM, RCS_ISP);
		v->AddExhaust(th_att_lin[9], 1.5, 0.16, _V(-1.84152, 1.846496, .984), _V(0, 0, 1), tex_rcs);

		th_att_lin[10] = v->CreateThruster(_V(6, 0, -10), _V(0, 0, -1), RCS_THRUST / 3, ph_RM, RCS_ISP);
		//AddExhaust(th_rcs[12], 1, .10, _V(1.68, -1.67, -.575), _V(.577, -.577, -.577));
		v->AddExhaust(th_att_lin[10], 1.5, 0.16, _V(1.841586, -1.8576, .984), _V(0, 0, 1), tex_rcs);

		th_att_lin[11] = v->CreateThruster(_V(-6, 0, -10), _V(0, 0, -1), RCS_THRUST / 3, ph_RM, RCS_ISP);
		*/
	th_att_lin[8] = v->CreateThruster(_V(6, 0, 10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	v->AddExhaust(th_att_lin[8], 1.5, 0.16, _V(2.13025, 0.063, -1.440188), _V(.7, 0, .7), tex_rcs);
	v->AddExhaust(th_att_lin[8], 1.5, 0.16, _V(2.13025, -0.057, -1.440188), _V(.7, 0, .7), tex_rcs);

	//OLD 7
	th_att_lin[9] = v->CreateThruster(_V(-6, 0, 10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[7], 1, .10, _V(-1.7589, .991, 9.6925), _V(0, 0, 1));
	v->AddExhaust(th_att_lin[9], 1.5, 0.16, _V(-2.13025, 0.063, -1.440188), _V(-.7, 0, .7), tex_rcs);
	v->AddExhaust(th_att_lin[9], 1.5, 0.16, _V(-2.13025, -0.057, -1.440188), _V(-.7, 0, .7), tex_rcs);
	//OLD 12
	th_att_lin[10] = v->CreateThruster(_V(6, 0, -10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//AddExhaust(th_rcs[12], 1, .10, _V(1.68, -1.67, -.575), _V(.577, -.577, -.577));
//	AddExhaust(th_rcs[12], 1.5, 0.16, _V(0.074, - 2.133988, - 1.837606), _V(0, -.7,-.7), tex_rcs);
	//AddExhaust(th_rcs[12], 1.5, 0.16, _V( -0.057, -2.13025, -1.440188), _V(0, -.7, .7), tex_rcs);
	v->AddExhaust(th_att_lin[10], 1.5, 0.16, _V(-.044, -2.154, -1.44), _V(0, -.7, .7), tex_rcs);
	v->AddExhaust(th_att_lin[10], 1.5, 0.16, _V(.077, -2.154, -1.44), _V(0, -.7, .7), tex_rcs);
	//OLD 17
	th_att_lin[11] = v->CreateThruster(_V(-6, 0, -10), _V(0, 0, -1), RCS_THRUSTTRANS, ph_RM, RCS_ISP);
	//	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-0.074, - 2.133988, - 1.837606), _V(0, -.7, -.7), tex_rcs);
	//	AddExhaust(th_rcs[17], 1.5, 0.16, _V(-0.057, 2.13025, -1.440188), _V(0, .7,.7), tex_rcs);
	v->AddExhaust(th_att_lin[11], 1.5, 0.16, _V(-.044, 2.154, -1.44), _V(0, .7, .7), tex_rcs);
	v->AddExhaust(th_att_lin[11], 1.5, 0.16, _V(.077, 2.154, -1.44), _V(0, .7, .7), tex_rcs);









	//	v->AddExhaust(th_att_lin[11], 1.5, 0.16, _V(-1.851452, -1.845157, .984), _V(0, 0, 1), tex_rcs);

	v->SetThrusterLevel(th_att_lin[8], 1.0);
	v->SetThrusterLevel(th_att_lin[9], 1.0);
	v->SetThrusterLevel(th_att_lin[10], 1.0);
	v->SetThrusterLevel(th_att_lin[11], 1.0);




	v->SetSize(6);
	v->SetCrossSections(_V(24, 28, 19));
	v->SetCW(0.5, 0.5, 1, 1);
	if (j)  return;
	/*
	// Hack to display the solar panel around the separated RM
	GetStatus(vs);
	vs.flag[0] = 3;
	ofs = _V(0, 0, -1.87);

	Local2Rel(ofs, vs.rpos);

	vel = _V(0, 0, -0.02);
	rofs, rvel = _V(vs.rvel.x, vs.rvel.y, vs.rvel.z);
	Local2Rel(ofs, vs.rpos);
	GlobalRot(vel, rofs);
	vs.rvel.x = rvel.x + rofs.x;
	vs.rvel.y = rvel.y + rofs.y;
	vs.rvel.z = rvel.z + rofs.z;

	//strcpy(name, "S");
	//h = oapiCreateVessel(name, "Orion-MPCV\\Orion-SMS", vs); // config file of created solar panel
	//v = oapiGetVesselInterface(h);

	v->SetEmptyMass(SM_DRYMASS);
	ph_RM = v->CreatePropellantResource(7.06, 7.06, 1.0);
	th_att_lin[0] = v->CreateThruster(_V(0, 0, 0), _V(0, 0, -1), 0.703 * 8 * RCS_THRUST / 3, ph_RM, RCS_ISP);

	v->SetThrusterLevel(th_att_lin[0], 1.0);

	v->SetSize(6);
	v->SetCrossSections(_V(24, 28, 19));
	v->SetCW(0.5, 0.5, 1, 1);
	//v->mode = 99;

	v->SetEnableFocus(false);
	*/
}
void ARTEMIS2::blowLas() {
	VESSELSTATUS vs;
	char name[256];
	PROPELLANT_HANDLE ph_las_aux;
	THRUSTER_HANDLE th_las_aux;
	OBJHANDLE h;
	VESSEL* v;

	if (oapiGetTimeAcceleration() > 10)		// Slow down
		oapiSetTimeAcceleration(10);

	//	SetEmptyMass (CM_DRYMASS+CM_FUEL+SM_DRYMASS+SM_FUEL);
//	ReloadMeshes();
	SetMeshVisibilityMode(15, MESHVIS_NEVER);
	GetStatus(vs);
	vs.flag[0] = 3;
	VECTOR3 ofs = { 0,0, mode == 12 ? 0 : 0 };
	Local2Rel(ofs, vs.rpos);

	VECTOR3 vel = _V(0, 0, 0.02);
	VECTOR3 rofs, rvel = { vs.rvel.x, vs.rvel.y, vs.rvel.z };
	GlobalRot(vel, rofs);
	vs.rvel.x = rvel.x + rofs.x;
	vs.rvel.y = rvel.y + rofs.y;
	vs.rvel.z = rvel.z + rofs.z;

	strcpy(name, GetName()); strcat(name, "-LAS");
	h = oapiCreateVessel(name, "ARMETISLAS", vs);
	v = oapiGetVesselInterface(h);

	v->SetEmptyMass(3696);
	ph_las_aux = v->CreatePropellantResource(480, 480, 1.0);
	th_las_aux = v->CreateThruster(_V(0.0, 0.0, 9.5), _V(0.0, 0.001, 1), 563250.0, ph_las_aux, LAS_ISP);
	v->SetThrusterLevel(th_las_aux, 1.0);
	v->SetEnableFocus(false);

	v->AddExhaust(th_las_aux, 8.0, 0.6, _V(0.0, 0.4, 12.3), _V(0, 0.4, -0.5), tex_rcs);
	v->AddExhaust(th_las_aux, 8.0, 0.6, _V(0.4, 0.0, 12.3), _V(0.4, 0, -0.5), tex_rcs);
	v->AddExhaust(th_las_aux, 8.0, 0.6, _V(0.0, -0.4, 12.3), _V(-0, -0.4, -0.5), tex_rcs);
	v->AddExhaust(th_las_aux, 8.0, 0.6, _V(-0.4, 0.0, 12.3), _V(-0.4, 0, -0.5), tex_rcs);

	v->SetPitchMomentScale(1e-5);
	v->SetBankMomentScale(1e-5);
}


