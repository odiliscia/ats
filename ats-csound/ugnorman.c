/* ATScsound Ugens, adapted by Alex Norman (2003) from the phase vocoder csound code by Richard Karpen
*  uses noise generations fucntions from atsh
*  the following needs to be put in entry.c

#include "ugnorman.h"

void    atsreadset(void*), atsread(void*);
void    atsreadnzset(void*), atsreadnz(void*);
void    atsaddset(void*), atsadd(void*);

taken out for now:
void    atsaddnzset(void*), atsaddnz(void*);
void    atsbufreadset(void*), atsbufread(void*);
void    atscrossset(void*), atscross(void*);
void    atsbufreadnzset(void*), atsbufreadnz(void*);
void    atscrossnzset(void*), atscrossnz(void*);

{ "atsread", S(ATSREAD), 3, "kk", "kSi", atsreadset, atsread, NULL},
{ "atsreadnz", S(ATSREADNZ), 3, "k", "kSi", atsreadnzset, atsreadnz, NULL},
{ "atsadd",    S(ATSADD),       5,     "a", "kkSiiopo", atsaddset,      NULL,   atsadd},
{ "atsaddnz",    S(ATSADDNZ),   5,     "a", "kSiop", atsaddnzset,     NULL,   atsaddnz},

taken out for now:
{ "atsbufread", S(ATSBUFREAD),  3,      "", "kkSiop", atsbufreadset, atsbufread, NULL},
{ "atscross", S(ATSCROSS),      5,      "a", "kkSikkiop", atscrossset,  NULL,   atscross},
{ "atsbufreadnz", S(ATSBUFREADNZ), 3,   "", "kS", atsbufreadnzset, atsbufreadnz, NULL},
{ "atscrossnz", S(ATSCROSSNZ),  5,      "a", "kSikkiop", atscrossnzset, NULL, atscrossnz}

put ugnorman.c ugnorman.h and ugnorman.o in the correct place in your make file as well.

function usage
see:
http://students.washington.edu/~anorman/atsadd.html
and
http://students.washington.edu/~anorman/atsread.html


read functions:

kfreq, kamp     atsread     ktimepnt, ifile, ipartial
kenergy atsreadnz    ktimepnt, ifile, iband

add functions:

ar      atsadd      ktimepnt, kfmod, iatsfile, ifn, ipartials[, ipartialoffset, ipartialincr, igatefn]
ar      atsaddnz    ktimepnt, iatsfile, ibands[, ibandoffset, ibandincr]

buf/cross functions:

        atsbufread      ktimepnt, kfmod, iatsfile, ipartials[, ipartialoffset, ipartialincr]
ar      atsadd      ktimepnt, kfmod, iatsfile, ifn, kmyamp, kbufamp, ipartials[, ipartialoffset, ipartialincr]

        atsbufreadnz    ktimepnt, iatsfile
ar      atscrossnz      ktimepnt, iatsfile, ifn, kmyamp, kbufamp, ibands[, ibandoffset, ibandincr]

*/


/* PROBLEM? for some reason on my system fopen causes seg fault if the file does not exist */

#include "cs.h"
// Use the below instead of the above if using as a plugin
//#include "csdl.h"

#include "ugnorman.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define ATSA_NOISE_VARIANCE 0.04


//extern	float	esr;	//the sampleing rate (i hope)
//extern	int	ksmps;
//extern	char	errmsg[];
//extern  int     odebug;

//static	int	pdebug = 0;

// static variables used for atsbufread and atsbufreadnz
static	ATSBUFREAD	*atsbufreadaddr;
static	ATSBUFREADNZ	*atsbufreadaddrnz;

int readdatafile(const char *, ATSFILEDATA *);

/************************************************************/
/*********  ATSREAD       ***********************************/
/************************************************************/

void FetchPartial(
	ATSREAD *p,
	float	*buf,
	float	position
	)
{
	float	frac;		// the distance in time we are between frames
	int	frame;		// the number of the first frame
	double * frm1, * frm2;	// a pointer to frame 1 and frame 2
	
	frame = (int)position;
	frm1 = p->datastart + p->frmInc * frame + p->partialloc;
	
	// if we're using the data from the last frame we shouldn't try to interpolate
	if(frame == p->maxFr)
	{
		buf[0] = (float)*frm1;	// calc amplitude
		buf[1] = (float)*(frm1 + 1); // calc freq
		return;
	}
	frm2 = frm1 + p->frmInc;
	frac = position - frame;
	
	buf[0] = (float)(*frm1 + frac * (*frm2 - *frm1));	// calc amplitude
	buf[1] = (float)(*(frm1 + 1) + frac * (*(frm2 + 1) - *(frm1 + 1))); // calc freq
}

void atsreadset(ATSREAD *p){
	char atsfilname[MAXNAME];
	ATSSTRUCT * atsh;
	
	/* copy in ats file name */
        if (*p->ifileno == sstrcod){
                strcpy(atsfilname, unquote(p->STRARG));
        }
        else if ((long)*p->ifileno < strsmax && strsets != NULL && strsets[(long)*p->ifileno])
                strcpy(atsfilname, strsets[(long)*p->ifileno]);
        else sprintf(atsfilname,"ats.%d", (int)*p->ifileno); /* else ats.filnum   */
	
	// load memfile
	if ( (p->atsmemfile = ldmemfile(atsfilname)) == NULL)
        {
                sprintf(errmsg, "ATSREAD: Ats file %s not read (does it exist?)", atsfilname);
                initerror(errmsg);
                return;
        }
	
	atsh = (ATSSTRUCT *)p->atsmemfile->beginp;
	//make sure that this is an ats file
	if (atsh->magic != 123)
	{
                sprintf(errmsg, "ATSREAD: either %s is not an ATS file or the byte endianness is wrong", atsfilname);
               	initerror(errmsg);
               	return;
        }

	p->maxFr = (int)atsh->nfrms - 1;
	p->timefrmInc = atsh->nfrms / atsh->dur;
		
	// check to see if partial is valid
	if( (int)(*p->ipartial) > (int)(atsh->npartials) || (int)(*p->ipartial) <= 0)
	{
		sprintf(errmsg, "ATSREAD: partial %i out of range, max allowed is %i", (int)(*p->ipartial), (int)(atsh->npartials));
		initerror(errmsg);
		return;
	}

	// point the data pointer to the correct partial
	p->datastart = (double *)(p->atsmemfile->beginp + sizeof(ATSSTRUCT));

        switch ( (int)(atsh->type))
        {

                case 1 :        p->partialloc = 1 + 2 * (*p->ipartial - 1);
                                p->frmInc = (int)(atsh->npartials * 2 + 1);
                                break;

                case 2 :        p->partialloc = 1 + 3 * (*p->ipartial - 1);
                                p->frmInc = (int)(atsh->npartials * 3 + 1);
                                break;

                case 3 :        p->partialloc = 1 + 2 * (*p->ipartial - 1);
                                p->frmInc = (int)(atsh->npartials * 2 + 26);
                                break;

                case 4 :        p->partialloc = 1 + 3 * (*p->ipartial - 1);
                                p->frmInc = (int)(atsh->npartials * 3 + 26);
                                break;
                default:        sprintf(errmsg, "Type not implemented");
                                initerror(errmsg);
                                return;
        }
	
	//flag set to reduce the amount of warnings sent out for time pointer out of range
	p->prFlg = 1;	// true
	return;
}

void atsread(ATSREAD *p){
	float frIndx;
	float buf[2];
	
	if(p->atsmemfile == NULL)
	{
		sprintf(errmsg, "ATSREAD: not initialized");
		initerror(errmsg);
		return;
	}
	if ( (frIndx = *(p->ktimpnt) * p->timefrmInc) < 0 )
	{
		frIndx = 0;
		if (p->prFlg)
		{
			p->prFlg = 0;	// set to false
			fprintf(stderr,"ATSREAD: only positive time pointer values allowed, setting to zero\n");
		}
		return;
	}
	else if (frIndx > p->maxFr)	// if we're trying to get frames past where we have data
	{
		frIndx = (float)p->maxFr;
		if (p->prFlg)
		{
			p->prFlg = 0;	// set to false
			fprintf(stderr,"ATSREAD: timepointer out of range, truncated to last frame\n");
		}
	}
	else
		p->prFlg = 1;
	
	FetchPartial(p, buf, frIndx);
	*p->kamp = buf[0];
	*p->kfreq = buf[1];
}

/*
 * ATSREADNOISE
 */
float FetchNzBand(ATSREADNZ *p, float	position)
{
	float	frac;		// the distance in time we are between frames
	int	frame;		// the time of the first frame
	double * frm1, * frm2;
	
	frame = (int)position;
	frm1 = p->datastart + p->frmInc * frame + p->nzbandloc;
	
	// if we're using the data from the last frame we shouldn't try to interpolate
	if(frame == p->maxFr)
		return (float)*frm1;
	
	frm2 = frm1 + p->frmInc;
	frac = position - frame;
	
	return (float)(*frm1 + frac * (*frm2 - *frm1));	// calc energy
}

void atsreadnzset(ATSREADNZ *p){
	char atsfilname[MAXNAME];
	ATSSTRUCT * atsh;
	
	/* copy in ats file name */
        if (*p->ifileno == sstrcod){
                strcpy(atsfilname, unquote(p->STRARG));
        }
        else if ((long)*p->ifileno < strsmax && strsets != NULL && strsets[(long)*p->ifileno])
                strcpy(atsfilname, strsets[(long)*p->ifileno]);
        else sprintf(atsfilname,"ats.%d", (int)*p->ifileno); /* else ats.filnum   */
	
	// load memfile
	if ( (p->atsmemfile = ldmemfile(atsfilname)) == NULL)
        {
                sprintf(errmsg, "ATSREADNZ: Ats file %s not read (does it exist?)", atsfilname);
                initerror(errmsg);
                return;
        }
	
	//point the header pointer at the header data
	atsh = (ATSSTRUCT *)p->atsmemfile->beginp;
	        
	//make sure that this is an ats file
	if (atsh->magic != 123)
	{
                sprintf(errmsg, "ATSREADNZ: either %s is not an ATS file or the byte endianness is wrong", atsfilname);
               	initerror(errmsg);
               	return;
        }
	p->maxFr = (int)atsh->nfrms - 1;
	p->timefrmInc = atsh->nfrms / atsh->dur;
	// point the data pointer to the correct partial
	p->datastart = (double *)(p->atsmemfile->beginp + sizeof(ATSSTRUCT));
		
	// check to see if band is valid
	if( (int)(*p->inzbin) > 25 || (int)(*p->inzbin) <= 0)
	{
		sprintf(errmsg, "ATSREADNZ: band %i out of range, 1-25 are the valid band values\n", (int)(*p->inzbin));
		initerror(errmsg);
		return;
	}

	switch ( (int)(atsh->type))
        {
                case 3 :        p->nzbandloc =  (int)(2 * atsh->npartials + *p->inzbin);        //get past the partial data to the noise
                                p->frmInc = (int)(atsh->npartials * 2 + 26);
                                break;

                case 4 :        p->nzbandloc = (int)(3 * atsh->npartials + *p->inzbin);
                                p->frmInc = (int)(atsh->npartials * 3 + 26);
                                break;
                default:        sprintf(errmsg, "ATSREADNZ: Type either not implemented or doesn't contain noise");
                                initerror(errmsg);
                                return;
        }
	//flag set to reduce the amount of warnings sent out for time pointer out of range
	p->prFlg = 1;	// true
	return;
}

void atsreadnz(ATSREADNZ *p){
	float frIndx;
	
	if(p->atsmemfile == NULL)
	{
		sprintf(errmsg, "ATSREADNZ: not initialized");
		initerror(errmsg);
		return;
	}
	// make sure we haven't over steped the bounds of the data
	if ( (frIndx = *(p->ktimpnt) * p->timefrmInc) < 0 )
	{
		frIndx = 0;
		if (p->prFlg)
		{
			p->prFlg = 0;	// set to false
			fprintf(stderr,"ATSREADNZ: only positive time pointer values allowed, setting to zero\n");
		}
		return;
	}
	else if (frIndx > p->maxFr)	// if we're trying to get frames past where we have data
	{
		frIndx = (float)p->maxFr;
		if (p->prFlg)
		{
			p->prFlg = 0;	// set to false
			fprintf(stderr,"ATSREADNZ: timepointer out of range, truncated to last frame\n");
		}
	}
	else
		p->prFlg = 1;
	*p->kenergy = FetchNzBand(p, frIndx);
}

/*
 * ATSADD
 */
void FetchADDPartials(ATSADD *, ATS_DATA_LOC *, float);
void AtsAmpGate(ATS_DATA_LOC *, int, FUNC *, double);

void atsaddset(ATSADD *p){
	char atsfilname[MAXNAME];
	ATSSTRUCT * atsh;
       	FUNC *ftp, *AmpGateFunc; 
	int i, memsize;
        
	// set up function table for synthesis
        if ((ftp = ftfind(p->ifn)) == NULL){
		sprintf(errmsg, "ATSADD: Function table number for synthesis waveform not valid\n");
		initerror(errmsg);
                return;
	}
        p->ftp = ftp;
	
	// set up gate function table
        if(*p->igatefun > 0){
                if ((AmpGateFunc = ftfind(p->igatefun)) == NULL)
		{
			sprintf(errmsg, "ATSADD: Gate Function table number not valid\n");
			initerror(errmsg);
                        return;
		}
                else
                        p->AmpGateFunc = AmpGateFunc;
        }
	
	/* copy in ats file name */
        if (*p->ifileno == sstrcod){
                strcpy(atsfilname, unquote(p->STRARG));
        }
        else if ((long)*p->ifileno < strsmax && strsets != NULL && strsets[(long)*p->ifileno])
                strcpy(atsfilname, strsets[(long)*p->ifileno]);
        else sprintf(atsfilname,"ats.%d", (int)*p->ifileno); /* else ats.filnum   */
	
	// load memfile
	if ( (p->atsmemfile = ldmemfile(atsfilname)) == NULL)
        {
                sprintf(errmsg, "ATSADD: Ats file %s not read (does it exist?)", atsfilname);
                initerror(errmsg);
                return;
        }
	//point the header pointer at the header data
	atsh = (ATSSTRUCT *)p->atsmemfile->beginp;
	
	//make sure that this is an ats file
	if (atsh->magic != 123)
	{
                sprintf(errmsg, "ATSREAD: either %s is not an ATS file or the byte endianness is wrong", atsfilname);
               	initerror(errmsg);
               	return;
        }
	//calculate how much memory we have to allocate for this
	memsize = (int)(atsh->nfrms) * (int)(*p->iptls) * sizeof(ATS_DATA_LOC) + (int)(*p->iptls) * sizeof(double);
	// allocate space if we need it
	if(p->auxch.auxp == NULL || memsize > p->memsize)
	{
		// need room for a buffer and an array of oscillator phase increments
		auxalloc(memsize, &p->auxch);
		p->memsize = memsize;
	}
	
	// set up the buffer, phase, etc.
	p->buf = (ATS_DATA_LOC *)(p->auxch.auxp);
	p->oscphase = (double *)(p->buf + (int)(*p->iptls));
	p->maxFr = (int)atsh->nfrms - 1;
	p->timefrmInc = atsh->nfrms / atsh->dur;
        p->MaxAmp = atsh->ampmax;        // store the maxium amplitude
	
	// make sure partials are in range
        if( (int)(*p->iptloffset + *p->iptls * *p->iptlincr)  > (int)(atsh->npartials) || (int)(*p->iptloffset) < 0)
        {
                sprintf(errmsg, "ATSADD: Partial(s) out of range, max partial allowed is %i", (int)atsh->npartials);
                initerror(errmsg);
                return;
        }
	//get a pointer to the beginning of the data
	p->datastart = (double *)(p->atsmemfile->beginp + sizeof(ATSSTRUCT));

	// get increments for the partials
	switch ( (int)(atsh->type))
        {
                case 1 :        p->firstpartial = (int)(1 + 2 * (*p->iptloffset));
                                p->partialinc = 2;
                                p->frmInc = (int)(atsh->npartials * 2 + 1);
                                break;

                case 2 :        p->firstpartial = (int)(1 + 3 * (*p->iptloffset));
                                p->partialinc = 3;
                                p->frmInc = (int)(atsh->npartials * 3 + 1);
                                break;

                case 3 :        p->firstpartial = (int)(1 + 2 * (*p->iptloffset));
                                p->partialinc = 2;
                                p->frmInc = (int)(atsh->npartials * 2 + 26);
                                break;

                case 4 :        p->firstpartial = (int)(1 + 3 * (*p->iptloffset));
                                p->partialinc = 3;
                                p->frmInc = (int)(atsh->npartials * 3 + 26);
                                break;

                default:        sprintf(errmsg, "ATSADD: Type not implemented");
                                initerror(errmsg);
                                return;
        }
	
	// initilize the phase of the oscilators
	for(i = 0; i < (int)(*p->iptls); i++)
		(p->oscphase)[i] = 0.0;
	
	//flag set to reduce the amount of warnings sent out for time pointer out of range
	p->prFlg = 1;	// true

	return;
}

void atsadd(ATSADD *p){
        float	frIndx;
        float	* ar, amp, fract, v1, *ftab;
        FUNC    *ftp;
        long    lobits, phase, inc;
        double *oscphase;
        int     i, nsmps = ksmps;
        int     numpartials = (int)*p->iptls;
	ATS_DATA_LOC * buf;
	buf = p->buf;

        if (p->auxch.auxp == NULL)
        {
		initerror("ATSADD: not intialized");
                return;
        }

        ftp = p->ftp;           // ftp is a poiter to the ftable
        if (ftp == NULL)
        {
                initerror("ATSADD: not intialized");
                return;
        }
	
	// make sure time pointer is within range
	if ( (frIndx = *(p->ktimpnt) * p->timefrmInc) < 0 )
        {
		frIndx = 0;
		if (p->prFlg)
		{
                	p->prFlg = 0;
			sprintf(errmsg, "ATSADD: only positive time pointer values are allowed, setting to zero\n");
			initerror(errmsg);
		}
        }
        else if (frIndx > p->maxFr) // if we're trying to get frames past where we have data
        {
                frIndx = (float)p->maxFr;
                if (p->prFlg)
                {
                        p->prFlg = 0;   // set to false
			sprintf(errmsg, "ATSADD: time pointer out of range, truncating to last frame\n");
			initerror(errmsg);
                }
        }
	else
		p->prFlg = 1;
        
	FetchADDPartials(p, buf, frIndx);
        
	oscphase = p->oscphase;
        // initialize output to zero
        ar = p->aoutput;
        for (i = 0; i < nsmps; i++)
                ar[i] = 0.0f;

        if(*p->igatefun > 0)
            AtsAmpGate(buf, *p->iptls , p->AmpGateFunc, p->MaxAmp);

        for (i = 0; i < numpartials; i++)
        {
                lobits = ftp->lobits;
                amp = (float)p->buf[i].amp;
                phase = (long)*oscphase;
                ar = p->aoutput;        // ar is a pointer to the audio output
                nsmps = ksmps;
                inc = (long)(p->buf[i].freq *  sicvt * *p->kfmod);  // put in * kfmod
                do {
                        ftab = ftp->ftable + (phase >> lobits);
                        v1 = *ftab++;
                        fract = (float) PFRAC(phase);
                        *ar += (v1 + fract * (*ftab - v1)) * amp;
                        ar++;
                        phase += inc;
                        phase &= PHMASK;
                }
                while (--nsmps);
                *oscphase = (double)phase;
                oscphase++;
        }
}

void FetchADDPartials(ATSADD *p, ATS_DATA_LOC *buf, float position)
{
        float   frac;           // the distance in time we are between frames
	double * frm0, * frm1;
        int     frame;
        int     i;      // for the for loop
	int     partialloc = p->firstpartial;
	int     npartials = (int)*p->iptls;
	
        frame = (int)position;
	frm0 = p->datastart + frame * p->frmInc;
	
	// if we're using the data from the last frame we shouldn't try to interpolate
	if(frame == p->maxFr)
	{
		for(i = 0; i < npartials; i++)
		{
			buf[i].amp = frm0[partialloc]; // calc amplitude
			buf[i].freq = frm0[partialloc + 1];
			partialloc += p->partialinc;
		}
		return;
	}
	
        frac = position - frame;
	frm1 = frm0 + p->frmInc;
	
        for(i = 0; i < npartials; i++)
        {
                buf[i].amp = frm0[partialloc] + frac * (frm1[partialloc] - frm0[partialloc]); // calc amplitude
		buf[i].freq = frm0[partialloc + 1] + frac * (frm1[partialloc + 1 ] - frm0[partialloc + 1]); // calc freq
		partialloc += p->partialinc;       // get to the next partial
        }
}

void AtsAmpGate(                // adaption of PvAmpGate by Richard Karpen
        ATS_DATA_LOC   *buf,       /* where to get our mag/freq pairs */
        int    npartials,      /* number of partials we're working with */
        FUNC    *ampfunc,
        double    MaxAmpInData
        )
{
        int    j;
        long    funclen, mapPoint;
        funclen = ampfunc->flen;

        for (j=0; j < npartials; ++j)   
        {
                /* use normalized amp as index into table for amp scaling */
                mapPoint = (long)((buf[j].amp / MaxAmpInData) * funclen);
                buf[j].amp *= (double)*(ampfunc->ftable + mapPoint);
	}
}

/************************************************************/
/*********  ATSADDNZ      ***********************************/
/************************************************************/

// copied directly from atsh synth-funcs.c with names changed so as not to conflict with csound
///////////////////////////////////////////////////////////////////
//randi output random numbers in the range of 1,-1
//getting a new number at frequency freq and interpolating
//the intermediate values.
void randiats_setup(float sr, float freq, RANDIATS *radat)
{
  long int first, second;

  first =random();
  second=random();

  radat->size= (int) (sr / freq) - 1;
  radat->a1  = first;
  radat->a2  = second;
  radat->cnt = 0;

  return;
}
///////////////////////////////////////////////////////////////////
float randiats(RANDIATS *radat)
{
  long int second; 
  float output;
 
  if(radat->cnt == radat->size) { //get a new random value
    radat->a1  = radat->a2; 
    second=random();
    radat->a2  = second;
    radat->cnt = 0;
  }

  output=(float)(((radat->a2 - radat->a1)/ radat->size) * radat->cnt)+ radat->a1;
  radat->cnt++;
  return(1. - ((float)(output /(long int)RAND_MAX) * 2.));
}
///////////////////////////////////////////////////////////////////
float randifats(RANDIATS *radat, float freq)
{
  long int second;
  float output;

  if(radat->cnt == radat->size) { //get a new random value
    radat->a1  = radat->a2;
    second=random();

    radat->a2  = second;
    radat->cnt = 0;
    radat->size= (int) (esr / freq) - 1;
  }

  output=(float)(((radat->a2 - radat->a1)/ radat->size) * radat->cnt)+ radat->a1;
  radat->cnt++;

  return(1. - ((float)(output /(long int)RAND_MAX) * 2.));
}

void FetchADDNZbands(
        double	*input[],
        double	*buf,
        float	position
        )
{
        float   frac;           // the distance in time we are between frames
        int     frame1, frame2;
        int     i;      // for the for loop

        frame1 = (int)position;
        frac = position - frame1;
	frame1 = frame1 * 25;	//the actual index of the array (a 1d array faking a 2d array)
	frame2 = frame1 + 25;	//get to the next frame
        for(i = 0; i < 25; i++)
        {
		buf[i] = *input[frame1 + i] + (double)frac * (*(input[frame2 + i]) - *(input[frame1 + i])); // calc energy
        }
}

void atsaddnzset(ATSADDNZ *p){
	char atsfilname[MAXNAME];
	ATSSTRUCT * atsh;
	int i, j, frmInc, firstband;
	double * temppnt;
	
	/* copy in ats file name */
        if (*p->ifileno == sstrcod){
                strcpy(atsfilname, unquote(p->STRARG));
        }
        else if ((long)*p->ifileno < strsmax && strsets != NULL && strsets[(long)*p->ifileno])
                strcpy(atsfilname, strsets[(long)*p->ifileno]);
        else sprintf(atsfilname,"ats.%d", (int)*p->ifileno); /* else ats.filnum   */
	
	if ( (p->atsmemfile = ldmemfile(atsfilname)) == NULL)
        {
                sprintf(errmsg, "ATSADDNZ: Ats file %s not read (does it exist?)", atsfilname);
                initerror(errmsg);
                return;
        }
	
	// allocate space so we can store the data for later use
	if(p->auxch.auxp == NULL || strcmp(p->filename, atsfilname) != 0)
	{
		// copy in the file name
		if(p->filename != NULL)
		{
			mfree(p->filename);
			p->filename = NULL;
		}
		p->filename = (char *)mmalloc(sizeof(char) * strlen(atsfilname));
		strcpy(p->filename, atsfilname);
		
		//point the header pointer at the header data
		atsh = (ATSSTRUCT *)p->atsmemfile->beginp;
	        
		//make sure that this is an ats file & byte endianness is correct
		if (atsh->magic != 123)
		{
        	        sprintf(errmsg, "ATSADDNZ: either %s is not an ATS file or the byte endianness is wrong", atsfilname);
                	initerror(errmsg);
                	return;
        	}
		
		// make sure that this file contains noise
		if(atsh->type != 4 && atsh->type != 3)
		{
			if (atsh->type < 5)
				sprintf(errmsg, "ATSADDNZ: This file type contains no noise");
			else
				sprintf(errmsg, "ATSADDNZ: This file type has not been implemented in this code yet.");
			initerror(errmsg);
			return;
		}

		auxalloc((int)((atsh->nfrms) * 25 * (sizeof(double *))), &p->auxch);
		
		p->datap = (double **) (p->auxch.auxp);
		//get past the last partial in the last frame of the data pointer;
		
		p->maxFr = atsh->nfrms - 1;
		p->timefrmInc = atsh->nfrms / atsh->dur;
		p->winsize = (float)atsh->winsz;
	}
	else
	{
		//point the header pointer at the header data
		atsh = (ATSSTRUCT *)p->atsmemfile->beginp;
	}
		
	// make sure partials are in range
        if( (int)(*p->ibandoffset + *p->ibands * *p->ibandincr)  > 25 || (int)(*p->ibandoffset) < 0)
        {
                sprintf(errmsg, "ATSADDNZ: Band(s) out of range, max band allowed is #25");
                initerror(errmsg);
                return;
        }

	// point the data pointer to the correct partials
	switch ( (int)(atsh->type))
        {
                case 3 :        firstband = 1 + 2 * (int)(atsh->npartials);
                                frmInc = (int)(atsh->npartials * 2 + 26);
                                break;
                
                case 4 :        firstband = 1 + 3 * (int)(atsh->npartials);
                                frmInc = (int)(atsh->npartials * 3 + 26);
                                break;
                
                default:        sprintf(errmsg, "Type either has no noise or is not implemented (only type 3 and 4 work now)");
                                initerror(errmsg);
                                return;
        }
	p->frmInc = frmInc;
	temppnt = (double *)(p->atsmemfile->beginp + sizeof(ATSSTRUCT));
	for(i = 0; i < (int)(atsh->nfrms); i++)
	{
		for(j = 0; j < 25; j++)
		{
			p->datap[j + i * 25] = (temppnt + firstband + j);
		}
		temppnt += frmInc;
	}
	
	// save bandwidths for creating noise bands
	p->nfreq[0] = 100;
	p->nfreq[1] = 100;
	p->nfreq[2] = 100;
	p->nfreq[3] = 100;
	p->nfreq[4] = 110;
	p->nfreq[5] = 120;
	p->nfreq[6] = 140;
	p->nfreq[7] = 150;
	p->nfreq[8] = 160;
	p->nfreq[9] = 190;
	p->nfreq[10] = 210;
	p->nfreq[11] = 240;
	p->nfreq[12] = 280;
	p->nfreq[13] = 320;
	p->nfreq[14] = 380;
	p->nfreq[15] = 450;
	p->nfreq[16] = 550;
	p->nfreq[17] = 700;
	p->nfreq[18] = 900;
	p->nfreq[19] = 1100;
	p->nfreq[20] = 1300;
	p->nfreq[21] = 1800;
	p->nfreq[22] = 2500;
	p->nfreq[23] = 3500;
	p->nfreq[24] = 4500;

	// initialize frequencies to modulate noise by
	p->phaseinc[0] = TWOPI * 50.0 / esr;
	p->phaseinc[1] = TWOPI * 150.0/ esr;
	p->phaseinc[2] = TWOPI * 250.0/ esr;
	p->phaseinc[3] = TWOPI * 350.0/ esr;
	p->phaseinc[4] = TWOPI * 455.0/ esr;
	p->phaseinc[5] = TWOPI * 570.0/ esr;
	p->phaseinc[6] = TWOPI * 700.0/ esr;
	p->phaseinc[7] = TWOPI * 845.0/ esr;
	p->phaseinc[8] = TWOPI * 1000.0/ esr;
	p->phaseinc[9] = TWOPI * 1175.0/ esr;
	p->phaseinc[10] = TWOPI * 1375.0/ esr;
	p->phaseinc[11] = TWOPI * 1600.0/ esr;
	p->phaseinc[12] = TWOPI * 1860.0/ esr;
	p->phaseinc[13] = TWOPI * 2160.0/ esr;
	p->phaseinc[14] = TWOPI * 2510.0/ esr;
	p->phaseinc[15] = TWOPI * 2925.0/ esr;
	p->phaseinc[16] = TWOPI * 3425.0/ esr;
	p->phaseinc[17] = TWOPI * 4050.0/ esr;
	p->phaseinc[18] = TWOPI * 4850.0/ esr;
	p->phaseinc[19] = TWOPI * 5850.0/ esr;
	p->phaseinc[20] = TWOPI * 7050.0/ esr;
	p->phaseinc[21] = TWOPI * 8600.0/ esr;
	p->phaseinc[22] = TWOPI * 10750.0/ esr;
	p->phaseinc[23] = TWOPI * 13750.0/ esr;
	p->phaseinc[24] = TWOPI * 17750.0/ esr;

	// initialize band limited noise parameters
	for(i = 0; i < 25; i++)
	{
		randiats_setup(esr, p->nfreq[i], &(p->randinoise[i]));
	}	
	
	//flag set to reduce the amount of warnings sent out for time pointer out of range
	p->prFlg = 1;	// true
	p->oscphase = 0;
	
	return;
}

void atsaddnz(ATSADDNZ *p){
	float frIndx;
	float *ar, amp;
	double *buf;
	int phase;
	float inc;	// a value to increment a phase index of the cosine by
	int	i, nsmps;
	int synthme;
	int nsynthed;

	if (p->auxch.auxp == NULL)
	{
		initerror("ATSADDNZ: not intialized");
		return;
	}

	// make sure time pointer is within range
	if ( (frIndx = *(p->ktimpnt) * p->timefrmInc) < 0 )
        {
		frIndx = 0;
		if (p->prFlg)
		{
                	p->prFlg = 0;
			sprintf(errmsg, "ATSADDNZ: only positive time pointer values are allowed, setting to zero\n");
			initerror(errmsg);
		}
        }
        else if (frIndx >= p->maxFr) // if we're trying to get frames past where we have data
        {
                frIndx = (float)p->maxFr - 1.0;
                if (p->prFlg)
                {
                        p->prFlg = 0;   // set to false
			sprintf(errmsg, "ATSADDNZ: time pointer out of range, truncating to last frame\n");
			initerror(errmsg);
                }
        }
	else
		p->prFlg = 1;
	
	buf = p->buf;
	
	FetchADDNZbands(p->datap, p->buf, frIndx);
	
	// set local pointer to output and initialize output to zero
	ar = p->aoutput;

	for (i = 0; i < ksmps; i++)
		*ar++ = 0.0f;
   	
	synthme = (int)*p->ibandoffset;
	nsynthed = 0;
	for (i = 0; i < 25; i++)
	{	
		// do we even have to synthesize it?
		if(i == synthme && nsynthed < (int)*p->ibands)
		{
			// synthesize cosine
			amp = (float)sqrt((p->buf[i] / ((p->winsize) * (float)ATSA_NOISE_VARIANCE) ) );
			phase = p->oscphase;
			ar = p->aoutput;
			nsmps = ksmps;
			inc = p->phaseinc[i];
			do
			{
				*ar += cos(inc * phase) * amp * randiats(&(p->randinoise[i]));
				phase += 1;
				ar++;
			}
			while(--nsmps);
			// make sure that the phase doesn't over flow
			/*
			while (phase >= costabsz)
				phase = phase - costabsz;
			*/
			nsynthed++;
			synthme += (int)*p->ibandincr;
		}	
	}
	p->oscphase = phase;
}

void atssinnoiset(ATSSINNOI *p)
{
}

void atssinnoi(ATSSINNOI *p)
{
}

/* the below is to allow this to be a plugin */

GLOBALS *pcglob;
#define S(x)       sizeof(x)

static OENTRY localops[] = {
  { "atsread", S(ATSREAD),  3, "kk", "kSi", atsreadset, atsread, NULL},
  { "atsreadnz", S(ATSREADNZ),  3, "k", "kSi", atsreadnzset, atsreadnz, NULL}
  //{ "atsadd",    S(ATSADD),	5,     "a", "kkSiiopo", atsaddset,      NULL,   atsadd}
};

long opcode_size(void)
{
    return sizeof(localops);
}

OENTRY *opcode_init(GLOBALS *xx)
{
    pcglob = xx;
    return localops;
}

