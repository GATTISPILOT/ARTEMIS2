const double EMPTY_MASS = 100.0;


const double ARM_OPERATING_SPEED = 0.01;
#define R_SIZE	2.0, 0.16		// SM RCS flame size
#define FR_SIZE	1.2, 0.12		// CM RCS flame size

#define MAIN_ENGSIZE 7, 1.0		// main engine flame size
#define AUX_SIZE	2.5, 0.4	// aux  engine flame size

#define RCS_THRUST		220	// SM RCS thrust [N]  LIFT OFF 2200000
#define RCS_THRUSTTRANS		440	// SM RCS thrust [N]  LIFT OFF 2200000

#define RCS_THRUST_LIFT_OFF		220000//2200000
#define MAIN_THRUST_LIFT_OFF 130000

#define AUX_THRUST		490		// SM Aux thrust
#define MAIN_THRUST	 	26700	// SM main thrust26700  LIFTOFF 130000
#define CRCS_THRUST		111		// CM RCS thrust

#define RCS_ISP			2900	// = 2650 + 10%	
#define MAIN_ISP	  	3400	// = 3100 + 10%
#define LAS_ISP			2700	// m/s

#define LAS_DRYMASS		5044	// kg
#define LAS_FUEL		2600	// kg

#define CM_DRYMASS		10220	// kg
#define CM_FUEL			170		// kg

#define SM_DRYMASS		5200	// kg incl CMA adapter
#define SM_FUEL			9200	// kg

#define FAIR_MASS		1360	// kg mass of all fairings
#define ADAPT_MASS		510		// kg

const double  LEM_DescentFUEL = 30978;				// Decent Stage fuel capacity [kg]
const double  LEM_DescentISP = 4398;				// Decent Engine exhuast velocity[m/s] 
const double  LEM_DescentTHRUST = 66.60e4;				// Decent Engine max thrust [n]

// Interface for derived vessel class: 
// ==========================================================
//const double ARM_OPERATING_SPEED = 0.3;//.05
const double FUELMASS = 1;

double MET, loadedMET;
int timer_state[10];
int hours1, minutes, minutes1, timenew, seconds, days, hours;


const double SOLP_SPEED = 0.04;

const VECTOR3 PANEL1CAM_POS = _V(1.574118, 1.253085, 6.525604E-02);
const VECTOR3 PANEL2CAM_POS = _V(-1.566905, 1.261168, 6.538782E-02);
const VECTOR3 PANEL3CAM_POS = _V(-1.7148, -1.620508, 6.527524E-02);
const VECTOR3 PANEL4CAM_POS = _V(1.4436, -1.161813, 6.527524E-02);



typedef struct {
	HINSTANCE hDLL;
	SURFHANDLE tkbk_label;
	HFONT font[2];
	//HFONT hFont;

	DWORD col[6];
	HBRUSH hBrush[5];

} GDIParams;

int mfd_buttons_ypos[3] = {
	64,
	320,
	576
};
#define ALL_MFD_BUTTONS 999999
#define CDR1_PWR	10199
#define CDR1_SEL	10299
#define CDR1_MNU	10399
#define PLT1_PWR	20199
#define PLT1_SEL	20299
#define PLT1_MNU	20399
#define CNTR1_PWR	30199
#define CNTR1_SEL	30299
#define CNTR1_MNU	30399
#define CDR1_LBUTTONS	101999
#define CDR1_RBUTTONS	102999
#define PLT1_LBUTTONS	201999
#define PLT1_RBUTTONS	202999
#define AID_CNTR_BRT	203999
#define AID_CNTR_DIM	204999




#define CNTR1_LBUTTONS	301999
#define CNTR1_RBUTTONS	302999

//MET
#define AID_METTIME         115
#define AID_METMODE         116
#define AID_GMTMODE         117




class ARTEMIS2 : public VESSEL4 {
public:
	ARTEMIS2(OBJHANDLE hObj, int fmodel);
	friend BOOL CALLBACK RMS_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void clbkPreStep(double simtt, double simdt, double mjd);
	
	void clbkSetClassCaps(FILEHANDLE cfg);
	int clbkConsumeBufferedKey(DWORD key, bool down, char *kstate);
	void clbkVisualCreated(VISHANDLE vis, int refcount);
	void DefineAnimations(void);
	void clbkVisualDestroyed(VISHANDLE vis, int refcount);
	void clbkPostStep(double simtt, double simdt, double mjd);
	void clbkLoadStateEx(FILEHANDLE scn, void *status);
	void clbkSaveState(FILEHANDLE scn);
    void CHANGESTATE();
	void RevertHATCH(void);
	void RevertTABLE(void);
	void RevertSPANELFOLD(void);
	
	enum HATCHStatus { HATCH_UP, HATCH_DOWN, HATCH_RAISING, HATCH_LOWERING } ARM3_status,  HATCH_status, SPANELFOLD_status, TABLE_status;
	enum SPANELcheck { SPANEL_UP, SPANEL_STOP, SPANEL_DOWN } ARM2_check, WINGROT1_check, WINGROT2_check, WINGROT3_check, WINGROT4_check;
    double ARM1_OPERATING_SPEED;
 

    void APPLYSKIN(void);

    void SetConfig0_Ready(void);
    void SetConfig1_Orbit(void);
    void SetConfig2_Reentry(void);
    void SetConfig3_Drogue(void);
    void SetConfig4_Chute(void);
    void SetConfig5_Chute_deployed(void);

    void SetConfig6_Land(void);
    void SetConfig7_Ready(void);
    void SetConfig10_Launch0(void);
    void SetConfig11_Launch1(void);

    void SetConfig12_Abort(void);

    void SetThrusters1_Orbit(void);
    void SetThrusters2_Reentry(void);
    void SetThrusters3_Land(void);
    void SetThrusters12_Abort(void);



    void blowSM(void);
    void blowFairing();
    void blowCover(void);
    void blowDrogue(void);
    void blowChute(void);
    void blowLas(void);



	//mfd
	bool clbkLoadVC(int id);
	bool clbkVCMouseEvent(int id, int event, VECTOR3& p);
	bool clbkVCRedrawEvent(int id, int event, SURFHANDLE surf);
	void clbkMFDMode(int mfd, int mode);
    void DoSolpRot(void);
	void DefineMFDButtons();

	void RedrawPanel_metmeterstatus(SURFHANDLE surf, int part);


    double t_0, t_message;
    THRUSTER_HANDLE th_main, th_descent[10];
    THRUSTER_HANDLE th_aux[8];		// RM auxillary engines
	double mfdbright[10];

	HFONT font = CreateFont(200, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");

	VCMFDSPEC cdr1_mfd;

	VCMFDSPEC plt1_mfd;

	VCMFDSPEC cntr1_mfd;

	//MET
	virtual VECTOR3 hms(double time);


	HINSTANCE hDLL;
	HFONT hFont, hFont1;
	HPEN hPen;
	HBRUSH hBrush;

    void PaintMFDButtons(SURFHANDLE surf);

    struct RECT_AREA {
        VECTOR3 pt1;
        VECTOR3 pt2;
        VECTOR3 pt3;
        VECTOR3 pt4;
    };

    RECT_AREA CDR1_PWR_AREA = {
        _V(-.565,  1.182, 2.8) ,
        _V(-.532,  1.182, 2.8),
        _V(-.565,  1.140, 2.8),
        _V(-.532,  1.140, 2.8)
    };
    RECT_AREA CDR1_SEL_AREA = {
        _V(-.305 , 1.182, 2.8) ,
        _V(-.25 , 1.182, 2.8),
        _V(-.305 , 1.14, 2.8),
        _V(-.25 , 1.14, 2.8)
    };
    RECT_AREA CDR1_MNU_AREA = {
        _V(-.2947945,  .846, 2.81) ,
        _V(-.267,  .846, 2.81),
        _V(-.2947945,  .8, 2.81),
        _V(-.267,  .8, 2.81)
    };

    RECT_AREA PLT1_PWR_AREA = {
        _V(.2648653,  1.182, 2.8) ,
        _V(.2887141,  1.182, 2.8),
        _V(.2648653,   1.14, 2.8),
        _V(.2887141,   1.14, 2.8)
    };
    RECT_AREA PLT1_SEL_AREA = {
        _V(.5339292,  1.182, 2.8) ,
        _V(.5559435,  1.182, 2.8),
        _V(.5339292,   1.14, 2.8),
        _V(.5559435,   1.14, 2.8)
    };
    RECT_AREA PLT1_MNU_AREA = {
        _V(.5327061,  1.077333, 2.7802) ,
        _V(.557778,  1.077333, 2.7802),
        _V(.5327061,  1.050427, 2.7802),
        _V(.557778,  1.050427, 2.7802)
    };

    RECT_AREA CNTR1_PWR_AREA = {
        _V(-.155,  1.182, 2.8) ,
        _V(-.109,  1.182, 2.8),
        _V(-.155,  1.14, 2.8),
        _V(-.109,  1.14, 2.8)
    };
    RECT_AREA CNTR1_SEL_AREA = {
        _V(.1191559,  1.182 ,2.745694) ,
        _V(.144,  1.182 ,2.745694),
        _V(.1191559,  1.14 ,2.745694),
        _V(.144,  1.14 ,2.745694)
    };
    RECT_AREA CNTR1_MNU_AREA = {
        _V(.1165249,  1.080745, 2.745694) ,
        _V(.148,  1.080745, 2.745694),
        _V(.1165249,  1.048, 2.745694),
        _V(.148,  1.048, 2.745694)
    };

    RECT_AREA CDR1_LBUTTONS_AREA = {
        _V(-.5565868 , 1.079523 , 2.81),//-.5560454  1.083313 2.817692
        _V(-.5392619 , 1.081147, 2.81),//-.5387204  1.083855 2.817692
        _V(-.555504,  .9051915, 2.81),//-.5555039  .9051919 2.817692
        _V(-.5408861,  .9051915, 2.81)
    };
    RECT_AREA CDR1_RBUTTONS_AREA = {
        _V(-.2856315,  1.079364, 2.81),
        _V(-.2699309,  1.079906,2.81),
        _V(-.283466,  .9055739, 2.81),
        _V(-.2693895,  .9050326, 2.81)
    };
    RECT_AREA PLT1_LBUTTONS_AREA = {
        _V(.2642538,  1.340893, 2.7802),
        _V(.2850451,  1.340893, 2.7802),
        _V(.2642538,  1.131146, 2.7802),
        _V(.2850451, 1.131146, 2.7802)
    };
    RECT_AREA PLT1_RBUTTONS_AREA = {
        _V(.5363753,  1.340893, 2.7802),
        _V(.5571665,  1.340893, 2.7802),
        _V(.5363753,1.131146,2.7802),
        _V(.5571665,1.131146, 2.7802)
    };
    RECT_AREA CNTR1_LBUTTONS_AREA = {
        _V(-.1465793,  1.08 ,2.81),
        _V(.128,  1.08 ,2.81),
        _V(-.1465793,  .905 ,2.81),
        _V(.128,  .905 ,2.814)
    };
    RECT_AREA CNTR1_RBUTTONS_AREA = {
        _V(.124418,  1.08, 2.81),
        _V(.143,  1.08, 2.81),
        _V(.124418,  .905, 2.81),
        _V(.143,  .905, 2.81)
    };


    NOTEHANDLE hNote;



	//
	MESHHANDLE MAINCAP, VCMESH, SM_MESH, COVERMESH, CHAIRS4MESH, ORANGECREWMESH, BLUE2CREW, CHAIRSSTOWED,  BURNTHEATSHIELD, DROGUECHUTEMESH, MAINCHUTE, FAIR1MESH, FAIR2MESH, FAIR3MESH, LASMESH, BALLONMESH, BLACKTILEMESH;

	double  ARM_OPERATING_SPEED, center_arm_t, JOINT1_proc, WING1ROTATE_proc, JOINT3_proc, JOINT4_proc, JOINT5_proc, JOINT6_proc, JOINT0_proc, ARM3_proc, SPANELFOLD_proc, WING2ROTATE_proc, WING3ROTATE_proc, WING4ROTATE_proc, TABLE_proc, HATCH_proc, LIFT_SPEED;

	int PANELDEPLOYED, panelstow, panel1stow, vccameracase;


	UINT anim_SPANEL, anim_JOINT2, anim_JOINT3, anim_JOINT4, anim_JOINT5, anim_JOINT6, anim_JOINT7, anim_JOINT0, anim_base0, anim_JOINT8, anim_HGA, anim_HATCH, anim_SPANELROT, anim_SPANELFOLD, anim_SPANEL1ROTATE, anim_SPANEL2ROTATE, anim_SPANEL3ROTATE, anim_SPANEL4ROTATE, anim_SPANEL1CAM, anim_SPANEL2CAM, anim_SPANEL3CAM, anim_SPANEL4CAM, anim_TIMERKNOB, anim_TABLE;
	
	MGROUP_ROTATE* SOLAR1, * SOLAR2, * SOLAR3, * SOLAR4, * SOLAR5, * SOLAR6, * SOLAR7, * SOLAR8, * SOLAR9, * SOLAR10, * SOLAR11, * SOLAR12, * SOLAR13, * SOLAR14, * SOLAR15, * SOLAR16, * SOLAR17, * SOLAR18, * SOLAR19, * SOLAR20, * SOLAR21, * SOLAR22, * SOLAR23, * SOLAR24, * SOLAR25, * SOLAR26;
	MGROUP_ROTATE* SOLAR1A, * SOLAR2A, * SOLAR3A, * SOLAR4A, * SOLAR5A, * SOLAR6A, * SOLAR7A, * SOLAR8A, * SOLAR9A, * SOLAR10A, * SOLAR11A, * SOLAR12A, * SOLAR13A, * SOLAR14A, * SOLAR15A, * SOLAR16A, * SOLAR17A, * SOLAR18A, * SOLAR19A, * SOLAR20A, * SOLAR21A, * SOLAR22A, * SOLAR23A, * SOLAR24A, * SOLAR25A, * SOLAR26A;

	VECTOR3 arm_tip[6], arm_tip1[6], arm_tip2[6], arm_tip3[6];

	bool center_arm, centerROT_arm;
	double db, simdt, centerROT_arm_t;


   private:
	   SURFHANDLE tex_rcs, tex_main, bs1, BURNT_tex, BURNT_tex1, BURNT_tex2;
       int BEACON, BLOWNFAIRING,  CAM;;
       VECTOR3 ofs, ofs_camera, ofs_astro[4], ofs_hud, ofs_ares, ofs_1, ofs_2, ofs_ummu;
       int mode;	// 0 = orbit config, CM + SM, solar closed
       PROPELLANT_HANDLE ph_main, ph_RM, ph_CM, ph_descent;

       int  SCAFFOLD, light_status,  dockinglight_status, lightON, CREW, CHAIRDEPLOY, CHAIRDEPLOY2;
       DEVMESHHANDLE dhMesh0, dhMesh1, dhMesh2, dhMesh3;

       OBJHANDLE fair1h, fair2h, fair3h, fairLh;	// stored for later disposal

       int skin_sw;
      
       double fairing, h;
       char string[256];

       double step_time;
       double apd, solp_proc, solp_rot_proc;
       int solp_rot_status, solp_status;
       VECTOR3 beaconpos[5];
       VECTOR3 beaconcol[5];
       BEACONLIGHTSPEC beacon[5];
       int  GMTmode, lTime, METmode, sGMTMillis, FGMTmode, FTestmode, FMETmode;
       int hours1, minutes, minutes1, timenew, seconds, days, hours;
       int sGMTDays, sGMTHours, sGMTSeconds, sGMTMinutes;
       SpotLight* docking_lightfront;
};