// This file is part of the aMule Project
//
// Copyright (c) 2004 aMule Project ( http://www.amule-project.net )
// Original author: Emilio Sandoz
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


// Unified handling of file I/O and dialog I/O for Preferences
// =================== Under Construction ====================

#ifdef __GNUG__
    #pragma implementation "PrefsUnifiedDlg.cpp"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif


#include <wx/object.h>
#include <wx/checkbox.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/colordlg.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/config.h>
#include <wx/tokenzr.h>

#include "amule.h"			// Needed for theApp
#include "otherfunctions.h"	// Needed for MakeFoldername
#include "PrefsUnifiedDlg.h"
#include "CTypedPtrList.h"	// Needed for CList
#include "EditServerListDlg.h"
#include "amuleDlg.h"
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "StatisticsDlg.h"	// Needed for graph parameters, colors
#include "IPFilter.h"		// Needed for CIPFilter
#include "SearchList.h"
#include "DownloadQueue.h"
#include "DirectoryTreeCtrl.h"	// Needed for CDirectoryTreeCtrl

// The following classes are used for table-driven access to user-definable settings,
// most of which appear in the Preferences dialog.  All other items in the preference file
// (e.g. window column widths) use the same mechanism and the same table for consistency.
//
// The Rse classes defined below are used only by class PrefsUnifiedDlg; they are here in 
// a .cpp file for now because it facilitates the migration from the exiting code, which 
// is an evolving, interactive process.  Also, most (all) methods are currently defined 
// inline because it speeds up the design process to not have to edit both definition 
// and implementation (most Rse methods are virtual and called from very few places, so 
// there is actually very little price to pay in terms of code size).
//
// Note: currently all prefs are stored in a common struct which gets filled before 
// instances of the classes that use them are created.  The code here does not depend on
// this struct, and eventually it may be more advantageous to move the prefs vars into
// the classes they really belong to, as static members (which do not require an instance).


class Rse: public wxObject {  // Remembered Settings Element - root class
friend class PrefsUnifiedDlg;
friend class RseInt;
public:
	Rse(int ID, const wxString& szIniName, const wxString& szIniSection=wxT("")) 
		: id(ID), strName(szIniName), szSection(szIniSection), pctrl(NULL), wxc(wxcNone), prseLink(NULL)  {}
	Rse(const wxString& szIniName, const wxString& szIniSection=wxT("")) 
		: id(0), strName(szIniName), szSection(szIniSection), pctrl(NULL), wxc(wxcNone), prseLink(NULL)  {}
	virtual ~Rse() {}

	virtual void LoadFromFile(wxConfigBase& ini)	{}
	virtual void SaveToFile(wxConfigBase& ini)  	{}
		
	void TransferToDlg() 					
	{ 
		SetPrevValue();   
		if (id!=0)  
			SetDlgValue(); 
	}
	
	void TransferFromDlg() 					
	{ 
		if (id!=0)  
			StoreDlgValue(); 
	}
	
	virtual void SetDlgValue() 				{}
	virtual void SetPrevValue()				{}
	virtual void RestorePrevValue()			{}
	virtual void StoreDlgValue()			{}
	virtual bool WasChanged()				{ return false; }
	virtual void SetWxControl(wxControl *pc){}
		
	int Id() 								{ return id; }
	virtual int GetMemValue()				{ return 0; }
	virtual wxString GetMemStringValue()	{ return wxT(""); }
	virtual void SetMemValue(int val)		{}
	virtual int GetCtrlValue()				{ return 0; }
	virtual void SetCtrlValue(int val)		{}
	virtual void SetCtrlValue(wxString str)	{}
	virtual void SetCtrlRange(int iMin, int iMax) {}
	virtual int GetDefaultValue()			{ return 0; }
	virtual int GetPrevValue()				{ return 0; }
	virtual int GetCtrlCount()				{ wxASSERT(false); return 0;}

	void Propagate()						{ if (prseLink!=NULL) prseLink->SetCtrlValue(GetCtrlValue()); }
	void SetLink(Rse *prse)					{ prseLink = prse;}
	virtual int IdLinkedTo()				{ return 0; }
		
	virtual void SetEnabled(bool Enable) {
		((wxTextCtrl*)pctrl)->Enable(Enable);		
	}
	
protected:
	enum wxcType { wxcNone, wxcCheck, wxcSlider, wxcSpin, wxcText, wxcChoice, wxcStatic, 
					wxcButton, wxcRadioButton, wxcRadioBox };	

	Rse()  {}

	int			id;				// item ID in the prefs dlg
	wxString	strName;		// name of item in the ini file
	wxString	szSection;		// (optional) section in the ini file
	wxControl	*pctrl;			// pointer to wxControl in dlg
	wxcType		wxc;			// the type of control
	Rse			*prseLink;		// points to a linked dlg item, e.g. a dynamic label
};



class RseBool: public Rse {
// Handles all bool and pseudo-bool (int8 and uint8 used like bool) preference vars
public:
	RseBool(int ID, bool& bSetting, const wxString& szIniName, bool bDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), pbSet(&bSetting), bDef(bDefault)  {}

	virtual void LoadFromFile(wxConfigBase& ini) {
		ini.Read( wxT("/") + szSection + wxT("/") + strName, pbSet, bDef );
	}
	
	virtual void SaveToFile(wxConfigBase& ini) {
		ini.Write( wxT("/") + szSection + wxT("/") + strName, *pbSet );
	}
	
	virtual void SetDlgValue() 				{ SetCtrlValue(*pbSet);  Propagate(); }
	virtual void SetPrevValue()				{ bPrev = *pbSet; }
	virtual void RestorePrevValue() 		{ *pbSet = bPrev; }
	virtual void StoreDlgValue() 			{ *pbSet = GetCtrlValue(); }
	virtual bool WasChanged()				{ return (*pbSet != bPrev); }
	
	virtual void SetWxControl(wxControl *pc)
	{ 
		pctrl=pc;  
		wxASSERT(pctrl->IsKindOf(CLASSINFO(wxCheckBox))); 
		wxc = wxcCheck;	
	}
	
	virtual int GetCtrlValue()				
	{ 
		return ((wxCheckBox *)pctrl)->GetValue(); 
	}
	
	virtual void SetCtrlValue(int val)		
	{ 
		((wxCheckBox *)pctrl)->SetValue( val ); 
	}
	
	virtual int GetMemValue()				{ return *pbSet; }
	virtual void SetMemValue(int val)		{ *pbSet = (val != 0); }
	
	virtual int GetDefaultValue()			{ return bDef; }
	virtual int GetPrevValue()				{ return bPrev; }
	
private:
	bool	*pbSet;
	bool	bDef;
	bool	bPrev;
};



class RseInt: public Rse {
// Handles all signed and unsigned integer preference vars
public:
	RseInt(int ID, sint8& iSetting, const wxString& szIniName, sint8 iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}

	RseInt(int ID, sint16& iSetting, const wxString& szIniName, sint16 iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}

	RseInt(int ID, sint32& iSetting, const wxString& szIniName, sint32 iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}

	RseInt(int ID, uint8& iSetting, const wxString& szIniName, uint8 iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}

	RseInt(int ID, uint16& iSetting, const wxString& szIniName, uint16 iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}

	RseInt(int ID, uint32& iSetting, const wxString& szIniName, uint32 iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}
#ifdef __WXMSW__
	RseInt(int ID, COLORREF& iSetting, const wxString& szIniName, COLORREF iDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}
#endif
//	RseInt(int ID, int& iSetting, const wxString& szIniName, int32 iDefault, const wxString& szIniSection)
//		: Rse(ID, szIniName, szIniSection), cb(sizeof(iSetting)), piSet(&iSetting), iDef(iDefault)  {}

	virtual void LoadFromFile(wxConfigBase& ini) {
		WriteMem( ini.Read( wxT("/") + szSection + wxT("/") + strName, iDef ) );
	}
	
	virtual void SaveToFile(wxConfigBase& ini) {
		ini.Write( wxT("/") + szSection + wxT("/") + strName, (long)ReadMem() );
	}
	
	virtual void RestorePrevValue() 		{ WriteMem(iPrev); }
	virtual void SetPrevValue()				{ iPrev = ReadMem(); }
	virtual void SetDlgValue() 				{ SetCtrlValue(ReadMem());  Propagate(); }
	virtual void StoreDlgValue() 			{ WriteMem(GetCtrlValue()); }
	virtual bool WasChanged()				{ return (ReadMem() != iPrev); }
	virtual int GetMemValue()				{ return ReadMem(); }
	virtual void SetMemValue(int val)		{ WriteMem(val); }
	virtual int GetDefaultValue()			{ return iDef; }
	virtual int GetPrevValue()				{ return iPrev; }
	
	virtual int GetCtrlValue()
	{
		switch (wxc) {
			case wxcChoice:		return ((wxChoice*)pctrl)->GetSelection();
			case wxcSlider:		return ((wxSlider *)pctrl)->GetValue(); 
			case wxcSpin:		return ((wxSpinCtrl *)pctrl)->GetValue(); 
			case wxcRadioBox:	return ((wxRadioBox *)pctrl)->GetSelection();
			case wxcRadioButton: {
				int val = 0;
				for (Rse* prse=this; prse!=NULL; prse=prse->prseLink) {
					if (((wxRadioButton*)(prse->pctrl))->GetValue()) {
						return val;
					} else {
						val++;
					}
				}
				return -1;  // should never get here if the buttons are set up correctly
			}
			default:	wxASSERT(false);		
		}
		return 0;  // stop the compiler from complaining about missing return from function
	}
	
	virtual void SetCtrlValue(int val) 
	{
		switch (wxc) {
			case wxcChoice:	
				((wxChoice*)pctrl)->SetSelection((val < ((wxChoice*)pctrl)->GetCount()) ? val : 1);
				break;
			case wxcSlider:	
				((wxSlider *)pctrl)->SetValue(val); 
				break;
			case wxcSpin:	
				((wxSpinCtrl*)pctrl)->SetValue(val);
				break;
			case wxcRadioBox:
				((wxRadioBox*)pctrl)->SetSelection(val);
				break;				
			case wxcRadioButton: 	
				for (Rse* prse=this; prse!=NULL; prse=prse->prseLink)
					((wxRadioButton*)(prse->pctrl))->SetValue((val-- == 0));
				break;
			default:
				wxASSERT(false);
				break;
		}
}
	
	virtual void SetCtrlRange(int iMin, int iMax) 
	{
		if (wxc==wxcSlider)
			((wxSlider *)pctrl)->SetRange(iMin, iMax); 
		else if (wxc==wxcSpin)
			((wxSpinCtrl*)pctrl)->SetRange(iMin, iMax);
		else
			wxASSERT(false);
	}
	
	virtual void SetWxControl(wxControl *pc)	
	{ 
		pctrl=pc;  
		if (pctrl->IsKindOf(CLASSINFO(wxSlider))) 
			wxc = wxcSlider; 
		else if (pctrl->IsKindOf(CLASSINFO(wxSpinCtrl))) 
			wxc = wxcSpin;
		else if (pctrl->IsKindOf(CLASSINFO(wxRadioButton))) 
			wxc = wxcRadioButton;
		else if (pctrl->IsKindOf(CLASSINFO(wxChoice))) 
			wxc = wxcChoice;
		else if (pctrl->IsKindOf(CLASSINFO(wxRadioBox)))
			wxc = wxcRadioBox;
		wxASSERT(wxc != wxcNone);
	}

	int GetCtrlCount() {
		if (wxc==wxcChoice) {
			return ((wxChoice *)pctrl)->GetCount();
		} else {
			wxASSERT(false);
			return 0;
		}
	}
	
private:
	void WriteMem(int32 i)
	{
		switch (cb) {
			case sizeof(int32): *(int32*)piSet = i;  break;
			case sizeof(int16): *(int16*)piSet = i;  break;
			case sizeof(int8):	*(int8*)piSet = i;   break;
			default:			wxASSERT(false);	 break;  
				// if this assert is triggered by a "int" on 64bit system, then
				// it's probably time to reduce the size of the underlying pref var
		}
	}

	int32 ReadMem() 
	{
		switch (cb) {
			case sizeof(int32):  return *(int32*)piSet;
			case sizeof(int16):  return *(int16*)piSet;
			case sizeof(int8):	 return *(int8*)piSet;
			default:			 wxASSERT(false);	  return 0;
		}
	}

	int			cb;
	void		*piSet;
	int32		iDef;
	int32		iPrev;
};



class RseDynLabel: public Rse {
// A special item type that creates a dynamic label for a slider or similar control.  No file I/O.
// It can appear anywhere in listRse, but it is recommended to put it immediately after
// the control it is linked to, for reasons of code maintainability.

public:
	RseDynLabel(int ID_label, 		// dlg ID of wxStaticText
			  int ID_ctrl, 			// dlg ID of linked control
			  int iMultiplier, 		// ctrl val * iMultiplier = output in string
			  wxString strSeveral, 	// format string to use for val*multiplier > 1
			  wxString strOne,	 	//							ditto == 1 ("" if not used)
			  wxString strZero)		//							ditto == 0 ("" if not used)
		: Rse(ID_label, wxT("label")), idLinkedTo(ID_ctrl), iMult(iMultiplier), 
			strSev(strSeveral), str1(strOne), str0(strZero)  {}
				
	virtual void SetWxControl(wxControl *pc)
	{ 
		pctrl=pc;  
		wxASSERT(pctrl->IsKindOf(CLASSINFO(wxStaticText))); 
		wxc = wxcStatic;	
	}

	virtual void SetCtrlValue(int val)		
	{
		int i = val * iMult;
		wxString str;
		
		if (i == 0) 
			str = (str0.IsEmpty()) ? str0 : strSev;
		else if (i == 1)
			str = (str1.IsEmpty()) ? str1 : strSev;
		else
			str = strSev;
		
		((wxStaticText*)pctrl)->SetLabel(wxString::Format(str, i));
	}
	
	virtual int IdLinkedTo()	{ return idLinkedTo; }
	
private:
	int	idLinkedTo;
	int iMult;
	wxString strSev;
	wxString str1;
	wxString str0;
};



class RseCounter: public Rse {
// Used for long-term statistics counters, e.g. for total bytes received; no dlg use
public:
	RseCounter(uint64& iSetting, const wxString&  szIniName, const wxString& szIniSection)
		: Rse(szIniName, szIniSection), piSet(&iSetting)  {}

	virtual void LoadFromFile(wxConfigBase& ini) { 
		wxString buffer;

		buffer = ini.Read( wxT("/") + szSection + wxT("/") + strName, wxT("0"));
		
		*piSet = atoll(unicode2char(buffer));
	}
	virtual void SaveToFile(wxConfigBase& ini)		{ 
		wxString str = wxString::Format(wxT("%llu"),(unsigned long long)*piSet);
		
		ini.Write( wxT("/") + szSection + wxT("/") + strName, str);
	}
	
private:
	uint64	*piSet;
};



class RseString: public Rse {
// all settable strings, e.g. name of video viewer
public:
	RseString(int ID, char * pchSetting, int cchMax, const wxString&  szIniName, const wxString& szDefault, const wxString& szIniSection)
		: Rse(ID, szIniName, szIniSection), pchSet(pchSetting), szDef(szDefault), cch(cchMax)  {}

	virtual void LoadFromFile(wxConfigBase& ini) {
		wxString buffer;

		buffer = ini.Read( wxT("/") + szSection + wxT("/") + strName, szDef);

		snprintf(pchSet, cch, "%s", unicode2char(buffer));
	}
	
	virtual void SaveToFile(wxConfigBase& ini) {
		ini.Write( wxT("/") + szSection + wxT("/") + strName, wxString(char2unicode(pchSet)));
	}

	virtual void SetDlgValue() 				
	{
		bWasChanged = false;
		if (wxc==wxcText)
			((wxTextCtrl*)pctrl)->SetValue(char2unicode(pchSet));
	}
	
	virtual void RestorePrevValue()			
	{
#ifdef __DEBUG__
		printf("restore on Cancel not yet supported in RseString\n");
#endif
	}
		
	virtual void StoreDlgValue()			
	{
		wxString test = ((wxTextCtrl*)pctrl)->GetValue();
		bWasChanged =  test.Cmp( char2unicode( pchSet ) );
		if (wxc==wxcText)
			snprintf(pchSet, cch, "%s", unicode2char(test));		
	}
		
	virtual bool WasChanged()					{ return bWasChanged; }
	
	virtual void SetCtrlValue(wxString str)		{ ((wxTextCtrl*)pctrl)->SetValue(str); }

	virtual void SetWxControl(wxControl *pc)
	{ 
		pctrl=pc;  
		if (pctrl->IsKindOf(CLASSINFO(wxTextCtrl)))
			wxc = wxcText; 
	}
	
	wxString GetMemStringValue() { wxASSERT(pchSet); return char2unicode(pchSet); }
	
protected:
	RseString() {};
	char	*pchSet;
	wxString szDef;
	int		cch;

private:
	char	*szPrev;
	bool	bWasChanged;
};

	

class RseStringEncrypted: public RseString {
// used for passwords, which should not appear in clear text on disk - no default value
// the password will be encrypted on disk, in the clear in memory.
public:
	RseStringEncrypted(int ID, char * pchSetting, int cchMax, const wxString&  szIniName, const wxString& szIniSection)
		: RseString(ID, pchSetting, cchMax, szIniName, wxT(""), szIniSection)  {}

	//shakraw, when storing value, store it encrypted here (only if changed in prefs)
	virtual void StoreDlgValue()
	{
		const char * sz = unicode2char(((wxTextCtrl*)pctrl)->GetValue());
		bWasChanged = (strcmp(pchSet, sz) != 0);
		if ((wxc==wxcText) && (bWasChanged))
			snprintf(pchSet, cch, "%s", unicode2char(MD5Sum(char2unicode(sz)).GetHash()));
	}
	
private:
	bool	bWasChanged;
};



class RseDirAssured: public RseString {
// A special string: the name of a directory which will be created if it does not already exist
// (used for Temp and Incoming directories) the name gets prepended with the app dir
public:
	RseDirAssured(int ID, char * pchSetting, const wxString&  szAppDir, const wxString&  szIniName, const wxString& szDefault, const wxString& szIniSection)
		: RseString(ID, pchSetting, MAX_PATH, szIniName, szDefault, szIniSection), strAppDir(szAppDir)  {}
		
	virtual void LoadFromFile(wxConfigBase& ini)  {
		wxString buffer;

		buffer = ini.Read( wxT("/") + szSection + wxT("/") + strName, strAppDir + szDef);

		buffer = MakeFoldername(buffer);
		
		sprintf(pchSet, "%s", unicode2char(buffer) );
	}
	
	void SelectDir()
	{
		wxString str = wxDirSelector(_("Choose a folder for ")+wxString(szDef), wxT(""));
		if (!str.IsEmpty())
			SetCtrlValue(str);
	}

private:
	wxString	strAppDir;
};	



class RseColumns: public Rse {  
// Window column widths / order / hidden state; used for .ini file only, no dlg interface.
// Note: the individual values here are declared as "int16", but the character sequence written
// to the .ini file for "default column width" is "65535", NOT "-1" as one would expect - this
// behavior appears to rely on a bug in CIni::SerGet, which now is a feature - we need to keep 
// doing it because we have always done it that way, and eMule does it that way.
public:
	RseColumns(int16 *piSetting, int count, const wxString&  szIniName, int16 iDefault, const wxString&  szIniSection)
		: Rse(szIniName, szIniSection), piSet(piSetting), iDef(iDefault), cnt(count)  {}

	virtual void LoadFromFile(wxConfigBase& ini) {
		wxString buffer;

		buffer = ini.Read( wxT("/") + szSection + wxT("/") + strName, wxT(""));
		
		for ( int i = 0; i < cnt; i++ )
			piSet[i] = iDef;
		
		
		int counter = 0;	
		wxStringTokenizer tokenizer( buffer, wxT(",") );
		while ( tokenizer.HasMoreTokens() && ( counter < cnt ) )
		{
    		wxString token = tokenizer.GetNextToken();

			piSet[counter] = atoi( unicode2char(token) );
			
			counter++;
		}

	}
	
	virtual void SaveToFile(wxConfigBase& ini) {
		wxString buffer;

		for ( int i = 0; i < cnt; i++ ) {
			if ( i ) 
				buffer << wxT(",");

			buffer << piSet[i];
		}
	
		
		ini.Write( wxT("/") + szSection + wxT("/") + strName, buffer );
	}
	
private:
	int16	*piSet;  
	int16	iDef;
	int		cnt;
};



//========================================================================================
//
// The following list drives loading/saving preferences in the ini file as well as getting
// values in and out of the new Preferences dialog.  The order in the list determines the 
// order in the ini file.  New items are normally added to the end of the list.
//
// When you decide to make variable XYZ in class ABC user-settable (i.e. making it a 
// "preference"), this is the recommended way of going about it:
//
// (1) make class PrefsUnifiedDlg a friend of class ABC to give it direct access;
// (2) make XYZ a "static" member of class ABC and put it in the "protected" section 
//     of ABC so that a friend class has access to it;
// (3) define XYZ in the class definition file and give it the desired default value;
// (4) use wxDesigner (or ask a team member to do this for you) to create a dialog control 
//     that will allow changing XYZ;  name the control something like IDC_XYZ and write 
//     down that ID for the next step;  generate the new muuli_wdr.cpp/.h pair;
// (5) add an entry of the appropriate type to the list below, using the ID from step (4) -
//     specify the value of the variable itself as the default value (so that the 
//     initialized value of the static member is the default)
//
// The basic idea is that "preferences" are just like other variables: even though we 
// make them persistent and give the user access to them, the principles of information
// hiding in object-oriented design still apply - variables should be defined where 
// they are normally used.  If you follow these recommendations then some day the aMule
// code will no longer be littered with references of the type "theApp.glob_prefs->xyz" ...
// [Emilio]
//
// WARNING:  do not, I repeat, DO NOT use casts like "(int32&)prefs->xyz" in the statements
// creating new table entries below.  Preventing the compiler from recognizing a variable
// according to its true type here may result in corruption of memory neighboring "prefs->xyz"!!!


// The list needs to be accessible before the dialog constructor gets called because we load 
// the preferences from file before setting up the rest of the app (so we use static methods).

//static 	CTypedPtrList<CPtrList, Rse*> listRse;

WX_DECLARE_LIST(Rse, ListOfRse);
#include <wx/listimpl.cpp>
WX_DEFINE_LIST(ListOfRse);

ListOfRse listRse;

Rse*	prseMaxUp;		// these pointers are needed before 
Rse*	prseMaxDown;	// PrefsUnifiedDlg constructor gets called
Rse*	aprseColor[cntStatColors];  // this array helps in accessing stat colors through RseInt's
    


void PrefsUnifiedDlg::BuildItemList(Preferences_Struct *prefs, const wxString appdir)  // gets called at init time
{
	listRse.Append(new Rse(wxT("Missing ID of dlg item in listRse")));  // LEAVE AT HEAD OF LIST - handles missing dlg IDs gracefully
	
	listRse.Append(new RseDirAssured(IDC_TEMPFILES, prefs->tempdir, appdir, wxT("TempDir"), wxT("Temp"), wxT("eMule")));
	listRse.Append(new RseString(IDC_NICK, prefs->nick, sizeof(prefs->nick), wxT("Nick"), wxT("http://www.aMule.org"), wxT("eMule")));
	listRse.Append(new RseDirAssured(IDC_INCFILES,prefs->incomingdir, appdir, wxT("IncomingDir"), wxT("Incoming"),wxT("eMule")));

	listRse.Append(prseMaxUp = new RseInt(IDC_MAXUP, prefs->maxupload, wxT("MaxUpload"), UNLIMITED, wxT("eMule"))); // see note in ForceUlDlRateCorrelation
	listRse.Append(prseMaxDown = new RseInt(IDC_MAXDOWN, prefs->maxdownload, wxT("MaxDownload"), UNLIMITED, wxT("eMule"))); // ditto

	listRse.Append(new RseInt(IDC_SLOTALLOC, prefs->slotallocation, wxT("SlotAllocation"), 2, wxT("eMule")));
	listRse.Append(new RseInt(IDC_MAXCON, prefs->maxconnections, wxT("MaxConnections"), CPreferences::GetRecommendedMaxConnections(), wxT("eMule")));
	listRse.Append(new RseBool(IDC_REMOVEDEAD, prefs->deadserver,wxT("RemoveDeadServer"), 1, wxT("eMule")));
	listRse.Append(new RseInt(IDC_PORT, prefs->port, wxT("Port"), 4662, wxT("eMule")));
	listRse.Append(new RseInt(IDC_UDPPORT, prefs->udpport, wxT("UDPPort"), 4672, wxT("eMule")));
	listRse.Append(new RseBool(IDC_UDPDISABLE, prefs->UDPDisable, wxT("UDPDisable"), false, wxT("eMule"))); 		
	listRse.Append(new RseInt(IDC_MAXSOURCEPERFILE, prefs->maxsourceperfile, wxT("MaxSourcesPerFile"), 300,wxT("eMule")));
	listRse.Append(new RseInt(IDC_LANGUAGE, prefs->languageID, wxT("Language"), 0,wxT("eMule")));

	listRse.Append(new RseInt(IDC_SEESHARES, prefs->m_iSeeShares, wxT("SeeShare"), 2,wxT("eMule")));
	listRse.Append(new RseInt(IDC_TOOLTIPDELAY, prefs->m_iToolDelayTime, wxT("ToolTipDelay"), 1, wxT("eMule")));

	listRse.Append(new RseInt(IDC_SLIDER, prefs->trafficOMeterInterval, wxT("StatGraphsInterval"), 3, wxT("eMule")));
	listRse.Append(new RseDynLabel(IDC_SLIDERINFO, IDC_SLIDER, 1, _("Update period: %i secs"), _("Update period: %i sec"), _("Update: Disabled")));
	listRse.Append(new RseInt(IDC_SLIDER2, prefs->statsInterval, wxT("statsInterval"), 30, wxT("eMule")));
	listRse.Append(new RseDynLabel(IDC_SLIDERINFO2, IDC_SLIDER2, 1, _("Update period: %i secs"), _("Update period: %i sec"), _("Update: Disabled")));

	listRse.Append(new RseInt(IDC_DOWNLOAD_CAP, prefs->maxGraphDownloadRate, wxT("DownloadCapacity"), 3, wxT("eMule"))); // see note in ForceUlDlRateCorrelation
	listRse.Append(new RseInt(IDC_UPLOAD_CAP, prefs->maxGraphUploadRate, wxT("UploadCapacity"), 3, wxT("eMule"))); // ditto
	listRse.Append(new RseInt(IDC_SERVERRETRIES, prefs->deadserverretries, wxT("DeadServerRetry"), 2, wxT("eMule")));

	listRse.Append(new RseInt(IDC_SERVERKEEPALIVE, prefs->m_dwServerKeepAliveTimeoutMins, wxT("ServerKeepAliveTimeout"), 0, wxT("eMule")));
	listRse.Append(new RseDynLabel(IDC_SERVERKEEPALIVE_LABEL, IDC_SERVERKEEPALIVE, 1,
		_("Server connection refresh interval %i mins"), _("Server connection refresh interval %i min"), _("Server connection refresh interval: Disabled")));

	listRse.Append(new RseInt(0, prefs->splitterbarPosition, wxT("SplitterbarPosition"), 75, wxT("eMule")));	// no GUI needed (window layout)


	listRse.Append(new RseInt(IDC_SLIDER4, prefs->statsMax, wxT("VariousStatisticsMaxValue"), 100, wxT("eMule")));	
	listRse.Append(new RseDynLabel(IDC_SLIDERINFO4, IDC_SLIDER4, 1, _("Connections Graph Scale: %i"), wxT(""), wxT("")));

	listRse.Append(new RseInt(IDC_SLIDER3, prefs->statsAverageMinutes, wxT("StatsAverageMinutes"), 5, wxT("eMule"))); 
	listRse.Append(new RseDynLabel(IDC_SLIDERINFO3, IDC_SLIDER3, 1, _("Time for running averages: %i mins"), wxT(""), wxT("")));

	listRse.Append(new RseInt(IDC_MAXCON5SEC, prefs->MaxConperFive, wxT("MaxConnectionsPerFiveSeconds"),prefs->MaxConperFive, wxT("eMule")));

	listRse.Append(new RseBool(IDC_RECONN, prefs->reconnect, wxT("Reconnect"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_SCORE, prefs->scorsystem, wxT("Scoresystem"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_ICH, prefs->ICH, wxT("ICH"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_AUTOSERVER, prefs->autoserverlist, wxT("Serverlist"), false, wxT("eMule")));

	listRse.Append(new RseString(0, prefs->m_szLRUServermetURL,sizeof(prefs->m_szLRUServermetURL), wxT("LRUServermetURL"), wxT(""), wxT("eMule"))); // no GUI yet

	listRse.Append(new RseBool(IDC_CHECK4UPDATE, prefs->updatenotify, wxT("UpdateNotify"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_MINTRAY, prefs->mintotray, wxT("MinToTray"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_UPDATESERVERCONNECT, prefs->addserversfromserver, wxT("AddServersFromServer"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_UPDATESERVERCLIENT, prefs->addserversfromclient, wxT("AddServersFromClient"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_SPLASHON, prefs->splashscreen, wxT("Splashscreen"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_BRINGTOFOREGROUND, prefs->bringtoforeground, wxT("BringToFront"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_DBLCLICK, prefs->transferDoubleclick, wxT("TransferDoubleClick"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_BEEPER, prefs->beepOnError, wxT("BeepOnError"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_EXIT, prefs->confirmExit, wxT("ConfirmExit"),false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_FILTER, prefs->filterBadIP, wxT("FilterBadIPs"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_AUTOCONNECT, prefs->autoconnect, wxT("Autoconnect"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_SHOWRATEONTITLE, prefs->showRatesInTitle, wxT("ShowRatesOnTitle"), false, wxT("eMule")));

	listRse.Append(new RseBool(IDC_ONLINESIG, prefs->onlineSig, wxT("OnlineSignature"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_STARTMIN, prefs->startMinimized, wxT("StartupMinimized"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_SAFESERVERCONNECT, prefs->safeServerConnect, wxT("SafeServerConnect"), false, wxT("eMule")));

	listRse.Append(new RseBool(0, prefs->filterserverbyip, wxT("FilterServersByIP"), false, wxT("eMule")));	// no GUI yet
	listRse.Append(new RseInt(0, prefs->filterlevel, wxT("FilterLevel"), 127, wxT("eMule")));					// no GUI yet
	listRse.Append(new RseBool(IDC_CHECKDISKSPACE, prefs->checkDiskspace, wxT("CheckDiskspace"), true, wxT("eMule")));			// no GUI yet
	listRse.Append(new RseInt(IDC_MINDISKSPACE, prefs->m_uMinFreeDiskSpace, wxT("MinFreeDiskSpace"), 1, wxT("eMule")));		// no GUI yet
	listRse.Append(new RseString(0, prefs->yourHostname,sizeof(prefs->yourHostname), wxT("YourHostname"), wxT(""), wxT("eMule"))); // no GUI yet

	listRse.Append(new RseBool(IDC_AUTOCONNECTSTATICONLY, prefs->autoconnectstaticonly, wxT("AutoConnectStaticOnly"), false, wxT("eMule"))); 
	listRse.Append(new RseBool(IDC_AUTOTAKEED2KLINKS, prefs->autotakeed2klinks, wxT("AutoTakeED2KLinks"), true, wxT("eMule"))); 
	listRse.Append(new RseBool(IDC_ADDNEWFILESPAUSED, prefs->addnewfilespaused, wxT("AddNewFilesPaused"), false, wxT("eMule"))); 
	listRse.Append(new RseInt(IDC_3DDEPTH, prefs->depth3D, wxT("3DDepth"), 10, wxT("eMule")));

	listRse.Append(new RseBool(IDC_CB_TBN_USESOUND, prefs->useSoundInNotifier, wxT("NotifierUseSound"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_CB_TBN_ONLOG, prefs->useLogNotifier, wxT("NotifyOnLog"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_CB_TBN_ONCHAT, prefs->useChatNotifier, wxT("NotifyOnChat"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_CB_TBN_POP_ALWAYS, prefs->notifierPopsEveryChatMsg, wxT("NotifierPopEveryChatMessage"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_CB_TBN_ONDOWNLOAD, prefs->useDownloadNotifier, wxT("NotifyOnDownload"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_CB_TBN_ONNEWVERSION, prefs->notifierNewVersion, wxT("NotifierPopNewVersion"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_CB_TBN_IMPORTATNT, prefs->notifierImportantError, wxT("NotifyOnImportantError"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_SENDMAIL, prefs->sendEmailNotifier, wxT("NotifyByMail"), false, wxT("eMule")));
	listRse.Append(new RseString(IDC_EDIT_TBN_WAVFILE, prefs->notifierSoundFilePath, sizeof(prefs->notifierSoundFilePath), wxT("NotifierSoundPath"),wxT(""), wxT("eMule")));

	listRse.Append(new RseString(0, prefs->notifierConfiguration, sizeof(prefs->notifierConfiguration), wxT("NotifierConfiguration"),wxT(""), wxT("eMule")));  // no GUI yet
	listRse.Append(new RseString(0, prefs->datetimeformat, sizeof(prefs->datetimeformat), wxT("DateTimeFormat"),wxT("%A, %x, %X"), wxT("eMule")));    // no GUI yet

/* We don't import irc shitty stuff from eMule so no need to handle ... */
	listRse.Append(new RseString(0, prefs->m_sircserver, sizeof(prefs->m_sircserver), wxT("DefaultIRCServer"), wxT("irc.emule-project.net"), wxT("eMule")));
	listRse.Append(new RseString(0, prefs->m_sircnick, sizeof(prefs->m_sircnick), wxT("IRCNick"), wxT("eMule"), wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bircaddtimestamp, wxT("IRCAddTimestamp"), true, wxT("eMule")));
	listRse.Append(new RseString(0, prefs->m_sircchannamefilter, sizeof(prefs->m_sircchannamefilter), wxT("IRCFilterName"), wxT(""), wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bircusechanfilter, wxT("IRCUseFilter"), false, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->m_iircchanneluserfilter, wxT("IRCFilterUser"), 0, wxT("eMule")));
	listRse.Append(new RseString(0, prefs->m_sircperformstring, sizeof(prefs->m_sircperformstring), wxT("IRCPerformString"), wxT("/join #emule"), wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bircuseperform, wxT("IRCUsePerform"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_birclistonconnect, wxT("IRCListOnConnect"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bircacceptlinks, wxT("IRCAcceptLinks"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bircignoreinfomessage, wxT("IRCIgnoreInfoMessage"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bircignoreemuleprotoinfomessage, wxT("IRCIgnoreEmuleProtoInfoMessage"), true, wxT("eMule")));
/* end of irc stuff */

	listRse.Append(new RseBool(IDC_SMARTIDCHECK, prefs->smartidcheck, wxT("SmartIdCheck"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_VERBOSE, prefs->m_bVerbose, wxT("Verbose"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_PREVIEWPRIO, prefs->m_bpreviewprio, wxT("PreviewPrio"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_MANUALSERVERHIGHPRIO, prefs->m_bmanualhighprio, wxT("ManualHighPrio"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_FULLCHUNKTRANS, prefs->m_btransferfullchunks, wxT("FullChunkTransfers"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_STARTNEXTFILE, prefs->m_bstartnextfile, wxT("StartNextFile"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_SHOWOVERHEAD, prefs->m_bshowoverhead, wxT("ShowOverhead"), false, wxT("eMule")));
	listRse.Append(new RseBool(IDC_VIDEOBACKUP, prefs->moviePreviewBackup, wxT("VideoPreviewBackupped"), true, wxT("eMule")));
	listRse.Append(new RseInt(IDC_FILEBUFFERSIZE, prefs->m_iFileBufferSize, wxT("FileBufferSizePref"), 16, wxT("eMule")));
	listRse.Append(new RseDynLabel(IDC_FILEBUFFERSIZE_STATIC, IDC_FILEBUFFERSIZE, 15000, _("File Buffer Size %i bytes"), wxT(""), wxT("")));
	
	listRse.Append(new RseInt(IDC_QUEUESIZE, prefs->m_iQueueSize, wxT("QueueSizePref"), 50, wxT("eMule")));
	listRse.Append(new RseDynLabel(IDC_QUEUESIZE_STATIC, IDC_QUEUESIZE, 100, _("Upload Queue Size %i clients"), wxT(""), wxT("")));
	listRse.Append(new RseInt(IDC_CHECKDAYS, prefs->versioncheckdays, wxT("Check4NewVersionDelay"), 5, wxT("eMule")));
	listRse.Append(new RseDynLabel(IDC_DAYS, IDC_CHECKDAYS, 1, _("%i days"), _("%i day"), wxT("")));
	listRse.Append(new RseBool(IDC_DAP, prefs->m_bDAP, wxT("DAPPref"), true, wxT("eMule")));
	listRse.Append(new RseBool(IDC_UAP, prefs->m_bUAP, wxT("UAPPref"), true, wxT("eMule")));

/* No traces of evidence for theses in old GUI prefs handling :-) */
	listRse.Append(new RseBool(0, prefs->indicateratings, wxT("IndicateRatings"), true, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->allcatType, wxT("AllcatType"), 0, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->showAllNotCats, wxT("ShowAllNotCats"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->watchclipboard, wxT("WatchClipboard4ED2kFilelinks"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->log2disk, wxT("SaveLogToDisk"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->debug2disk, wxT("SaveDebugToDisk"), false, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->iMaxLogMessages, wxT("MaxLogMessages"), 1000, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->resumeSameCat, wxT("ResumeNextFromSameCat"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->resumeSameCat, wxT("DontRecreateStatGraphsOnResize"), false, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->versioncheckLastAutomatic, wxT("VersionCheckLastAutomatic"), 0, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bDisableKnownClientList, wxT("DisableKnownClientList"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bDisableQueueList, wxT("DisableQueueList"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->m_bCreditSystem, wxT("UseCreditSystem"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->scheduler, wxT("EnableScheduler"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->msgonlyfriends, wxT("MessagesFromFriendsOnly"), false, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->msgsecure, wxT("MessageFromValidSourcesOnly"), true, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->maxmsgsessions, wxT("MaxMessageSessions"), 50, wxT("eMule")));
	listRse.Append(new RseString(0, prefs->TxtEditor, sizeof(prefs->TxtEditor), wxT("TxtEditor"), wxT(""), wxT("eMule")));
	listRse.Append(new RseString(0, prefs->m_sTemplateFile, sizeof(prefs->m_sTemplateFile), wxT("WebTemplateFile"), wxT("eMule.tmpl"), wxT("eMule")));
	listRse.Append(new RseString(0, prefs->messageFilter, sizeof(prefs->messageFilter), wxT("MessageFilter"), wxT("Your client has an infinite queue"), wxT("eMule")));
	listRse.Append(new RseString(0, prefs->commentFilter, sizeof(prefs->commentFilter), wxT("CommentFilter"), wxT("http://"), wxT("eMule")));

	listRse.Append(new RseString(IDC_VIDEOPLAYER, prefs->VideoPlayer, sizeof(prefs->VideoPlayer), wxT("VideoPlayer"), wxT(""), wxT("eMule")));
	listRse.Append(new RseBool(IDC_EXTCATINFO, prefs->showCatTabInfos, wxT("ShowInfoOnCatTabs"), false, wxT("eMule")));
	
/* window colum widths, no dialog interaction - BEGIN */
	listRse.Append(new RseColumns(prefs->downloadColumnWidths, ELEMENT_COUNT(prefs->downloadColumnWidths), wxT("DownloadColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->downloadColumnHidden, ELEMENT_COUNT(prefs->downloadColumnHidden), wxT("DownloadColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->downloadColumnOrder, ELEMENT_COUNT(prefs->downloadColumnOrder), wxT("DownloadColumnOrder"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->uploadColumnWidths, ELEMENT_COUNT(prefs->uploadColumnWidths), wxT("UploadColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->uploadColumnHidden, ELEMENT_COUNT(prefs->uploadColumnHidden), wxT("UploadColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->uploadColumnOrder, ELEMENT_COUNT(prefs->uploadColumnOrder), wxT("UploadColumnOrder"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->queueColumnWidths, ELEMENT_COUNT(prefs->queueColumnWidths), wxT("QueueColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->queueColumnHidden, ELEMENT_COUNT(prefs->queueColumnHidden), wxT("QueueColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->queueColumnOrder, ELEMENT_COUNT(prefs->queueColumnOrder), wxT("QueueColumnOrder"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->searchColumnWidths, ELEMENT_COUNT(prefs->searchColumnWidths), wxT("SearchColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->searchColumnHidden, ELEMENT_COUNT(prefs->searchColumnHidden), wxT("SearchColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->searchColumnOrder, ELEMENT_COUNT(prefs->searchColumnOrder), wxT("SearchColumnOrder"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->sharedColumnWidths, ELEMENT_COUNT(prefs->sharedColumnWidths), wxT("SharedColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->sharedColumnHidden, ELEMENT_COUNT(prefs->sharedColumnHidden), wxT("SharedColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->sharedColumnOrder, ELEMENT_COUNT(prefs->sharedColumnOrder), wxT("SharedColumnOrder"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->serverColumnWidths, ELEMENT_COUNT(prefs->serverColumnWidths), wxT("ServerColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->serverColumnHidden, ELEMENT_COUNT(prefs->serverColumnHidden), wxT("ServerColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->serverColumnOrder, ELEMENT_COUNT(prefs->serverColumnOrder), wxT("ServerColumnOrder"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->clientListColumnWidths, ELEMENT_COUNT(prefs->clientListColumnWidths), wxT("ClientListColumnWidths"), DEFAULT_COL_SIZE, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->clientListColumnHidden, ELEMENT_COUNT(prefs->clientListColumnHidden), wxT("ClientListColumnHidden"),0, wxT("eMule")));
	listRse.Append(new RseColumns(prefs->clientListColumnOrder, ELEMENT_COUNT(prefs->clientListColumnOrder), wxT("ClientListColumnOrder"),0, wxT("eMule")));
/*  window colum widths - END */

	// Barry - Provide a mechanism for all tables to store/retrieve sort order
	listRse.Append(new RseInt(0, prefs->tableSortItemDownload, wxT("TableSortItemDownload"), 0, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->tableSortItemUpload, wxT("TableSortItemUpload"), 0, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->tableSortItemQueue, wxT("TableSortItemQueue"), 0, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->tableSortItemSearch, wxT("TableSortItemSearch"), 0, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->tableSortItemShared, wxT("TableSortItemShared"), 0, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->tableSortItemServer, wxT("TableSortItemServer"), 0, wxT("eMule")));
	listRse.Append(new RseInt(0, prefs->tableSortItemClientList, wxT("TableSortItemClientList"), 0, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingDownload, wxT("TableSortAscendingDownload"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingUpload, wxT("TableSortAscendingUpload"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingQueue, wxT("TableSortAscendingQueue"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingSearch, wxT("TableSortAscendingSearch"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingShared, wxT("TableSortAscendingShared"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingServer, wxT("TableSortAscendingServer"), true, wxT("eMule")));
	listRse.Append(new RseBool(0, prefs->tableSortAscendingClientList, wxT("TableSortAscendingClientList"), true, wxT("eMule")));

	// deadlake PROXYSUPPORT - no GUI in aMule yet
	listRse.Append(new RseBool(0, prefs->proxy.EnablePassword, wxT("ProxyEnablePassword"), false, wxT("Proxy")));
	listRse.Append(new RseBool(0, prefs->proxy.UseProxy, wxT("ProxyEnableProxy"), false, wxT("Proxy")));
	listRse.Append(new RseString(0, prefs->proxy.name, sizeof(prefs->proxy.name), wxT("ProxyName"), wxT(""), wxT("Proxy")));
	listRse.Append(new RseStringEncrypted(0, prefs->proxy.password, sizeof(prefs->proxy.password), wxT("ProxyPassword"), wxT("Proxy")));
	listRse.Append(new RseString(0, prefs->proxy.user, sizeof(prefs->proxy.user), wxT("ProxyUser"), wxT(""), wxT("Proxy")));
	listRse.Append(new RseInt(0, prefs->proxy.port, wxT("ProxyPort"), 1080, wxT("Proxy")));
	listRse.Append(new RseInt(0, prefs->proxy.type, wxT("ProxyType"), PROXYTYPE_NOPROXY, wxT("Proxy")));

	for (int i=0; i<cntStatColors; i++) {  // colors have been moved from global prefs to CStatisticsDlg
		wxString str = wxString::Format(wxT("StatColor%i"),i);
		listRse.Append(aprseColor[i] = new RseInt(0, CStatisticsDlg::acrStat[i], str, CStatisticsDlg::acrStat[i], wxT("eMule")));
	}

	listRse.Append(new RseCounter(prefs->totalDownloadedBytes, wxT("TotalDownloadedBytes"), wxT("Statistics"))); // no GUI needed
	listRse.Append(new RseCounter(prefs->totalUploadedBytes, wxT("TotalUploadedBytes"), wxT("Statistics")));		// no GUI needed
	
	listRse.Append(new RseInt(0, prefs->desktopMode, wxT("DesktopMode"), 4, wxT("Statistics")));

	listRse.Append(new RseStringEncrypted(IDC_WEB_PASSWD, prefs->m_sWebPassword, sizeof(prefs->m_sWebPassword), wxT("Password"), wxT("WebServer")));
	listRse.Append(new RseStringEncrypted(IDC_WEB_PASSWD_LOW, prefs->m_sWebLowPassword, sizeof(prefs->m_sWebLowPassword), wxT("PasswordLow"), wxT("WebServer")));
	listRse.Append(new RseInt(IDC_WEB_PORT, prefs->m_nWebPort, wxT("Port"), 4711, wxT("WebServer")));
	listRse.Append(new RseBool(IDC_ENABLE_WEB, prefs->m_bWebEnabled, wxT("Enabled"), false, wxT("WebServer")));
	listRse.Append(new RseBool(IDC_WEB_GZIP, prefs->m_bWebUseGzip, wxT("UseGzip"), true, wxT("WebServer")));
	listRse.Append(new RseBool(IDC_ENABLE_WEB_LOW, prefs->m_bWebLowEnabled, wxT("UseLowRightsUser"), false, wxT("WebServer")));
	listRse.Append(new RseInt(IDC_WEB_REFRESH_TIMEOUT, prefs->m_nWebPageRefresh, wxT("PageRefreshTime"), 120, wxT("WebServer")));

	listRse.Append(new RseBool(0, prefs->dontcompressavi, wxT("DontCompressAvi"), false, wxT("WebServer")));  // no GUI yet

	listRse.Append(new RseInt(IDC_EXTENDED_DROP, prefs->NoNeededSources, wxT("NoNeededSources"), 1, wxT("Razor_Preferences")));

	listRse.Append(new RseBool(IDC_ENABLE_AUTO_FQS, prefs->DropFullQueueSources, wxT("FullQueueSources"), false, wxT("Razor_Preferences")));
	listRse.Append(new RseBool(IDC_ENABLE_AUTO_HQRS, prefs->DropHighQueueRankingSources, wxT("HighQueueRankingSources"), false, wxT("Razor_Preferences")));
	listRse.Append(new RseInt(IDC_HQR_VALUE, prefs->HighQueueRanking, wxT("HighQueueRanking"), 1200, wxT("Razor_Preferences")));
	listRse.Append(new RseInt(IDC_AUTO_DROP_TIMER, prefs->AutoDropTimer, wxT("AutoDropTimer"), 240, wxT("Razor_Preferences")));
	listRse.Append(new RseBool(IDC_FED2KLH, prefs->FastED2KLinksHandler, wxT("FastED2KLinksHandler"), true, wxT("Razor_Preferences")));

	listRse.Append(new RseBool(IDC_EXT_CONN_ACCEPT, prefs->AcceptExternalConnections, wxT("AcceptExternalConnections"), true,wxT("ExternalConnect")));
	listRse.Append(new RseBool(IDC_EXT_CONN_USETCP, prefs->ECUseTCPPort, wxT("ECUseTCPPort"), false,wxT("ExternalConnect")));
	listRse.Append(new RseInt(IDC_EXT_CONN_TCP_PORT, prefs->ECPort, wxT("ECPort"), 4712, wxT("ExternalConnect")));
	listRse.Append(new RseStringEncrypted(IDC_EXT_CONN_PASSWD, prefs->ECPassword, sizeof(prefs->ECPassword), wxT("ECPassword"), wxT("ExternalConnect")));
	listRse.Append(new RseBool(IDC_NEWSTYLETABS, prefs->bDlgTabsOnTop, wxT("DlgTabsOnTop"), false,wxT("ExternalConnect")));  

	// Kry
	listRse.Append(new RseBool(IDC_SECIDENT, prefs->SecIdent, wxT("UseSecIdent"), true,wxT("ExternalConnect")));
	listRse.Append(new RseBool(IDC_IPFONOFF, prefs->IPFilterOn, wxT("IpFilterOn"), true,wxT("ExternalConnect"))); 	 
	listRse.Append(new RseBool(IDC_SRCSEEDS, prefs->UseSrcSeeds, wxT("UseSrcSeeds"), false,wxT("ExternalConnect"))); 	 
	listRse.Append(new RseBool(IDC_PROGBAR, prefs->ProgBar, wxT("ShowProgressBar"), true,wxT("ExternalConnect"))); 	 
	listRse.Append(new RseBool(IDC_PERCENT, prefs->Percent, wxT("ShowPercent"), false,wxT("ExternalConnect"))); 	
	listRse.Append(new RseBool(IDC_METADATA, prefs->ExtractMetaData, wxT("ExtractMetaDataTags"), false,wxT("ExternalConnect"))); 	
	listRse.Append(new RseBool(IDC_CHUNKALLOC, prefs->AllocFullChunk, wxT("FullChunkAlloc"), false,wxT("ExternalConnect"))); 		
	listRse.Append(new RseBool(IDC_FULLALLOCATE, prefs->AllocFullPart, wxT("FullPartAlloc"), false,wxT("ExternalConnect"))); 		
	listRse.Append(new RseBool(IDC_FCHECKTABS, prefs->BrowserTab, wxT("BrowserTab"), true, wxT("FakeCheck")));
	listRse.Append(new RseString(IDC_FCHECKSELF, prefs->CustomBrowser, sizeof(prefs->CustomBrowser), wxT("CustomBrowser"), wxT(""), wxT("FakeCheck")));
	listRse.Append(new RseInt(IDC_FCHECK, prefs->Browser, wxT("Browser"), 0,wxT("FakeCheck")));	
	listRse.Append(new RseBool(IDC_SAFEMAXCONN, prefs->UseSafeMaxConn, wxT("SafeMaxConn"), false, wxT("FakeCheck"))); 		
	listRse.Append(new RseBool(IDC_VERBOSEPACKETERROR, prefs->VerbosePacketError, wxT("VerbosePacketError"), false, wxT("FakeCheck"))); 
	listRse.Append(new RseDirAssured(IDC_OSDIR, prefs->OSDirectory, appdir, wxT("OSDirectory"), wxT(""), wxT("FakeCheck")));	
	listRse.Append(new RseBool(/*IDC_USESKIN*/0, prefs->UseSkinFile, wxT("UseSkinFile"), false, wxT("MiscGUIOptions"))); 
	listRse.Append(new RseDirAssured(/*IDC_SKINFILE*/0, prefs->SkinFile, appdir, wxT("SkinFile"), wxT(""), wxT("MiscGUIOptions")));	
}

//==============================================================================
//
//  Dialog implementation

// WDR: class implementations

//----------------------------------------------------------------------------
// PrefsUnifiedDlg
//----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(PrefsUnifiedDlg,wxDialog)

// WDR: event table for PrefsUnifiedDlg

BEGIN_EVENT_TABLE(PrefsUnifiedDlg,wxDialog)
	EVT_SCROLL(PrefsUnifiedDlg::OnScroll)
	EVT_SPINCTRL(IDC_MAXUP, PrefsUnifiedDlg::OnSpinMaxDLR)
	EVT_SPINCTRL(IDC_MAXDOWN, PrefsUnifiedDlg::OnSpinMaxDLR)
	EVT_CHECKBOX(IDC_UDPDISABLE, PrefsUnifiedDlg::OnCheckBoxChange)
	
	EVT_BUTTON(ID_PREFS_OK_TOP, PrefsUnifiedDlg::OnOk)
	EVT_BUTTON(ID_PREFS_OK_LEFT, PrefsUnifiedDlg::OnOk)
	EVT_BUTTON(ID_OK, PrefsUnifiedDlg::OnOk)
	
	EVT_BUTTON(ID_PREFS_CANCEL_TOP, PrefsUnifiedDlg::OnCancel)
	EVT_BUTTON(ID_PREFS_CANCEL_LEFT, PrefsUnifiedDlg::OnCancel)
	EVT_BUTTON(ID_CANCEL, PrefsUnifiedDlg::OnCancel)
	
	EVT_BUTTON(IDC_SELTEMPDIR, PrefsUnifiedDlg::OnButtonDir)
	EVT_BUTTON(IDC_SELINCDIR,  PrefsUnifiedDlg::OnButtonDir)
	EVT_BUTTON(IDC_SELOSDIR,  PrefsUnifiedDlg::OnButtonDir)
	EVT_BUTTON(IDC_BTN_BROWSE_WAV, PrefsUnifiedDlg::OnButtonBrowseWav)
	EVT_BUTTON(IDC_BROWSEV, PrefsUnifiedDlg::OnButtonBrowseVideoplayer)
	EVT_BUTTON(IDC_EDITADR, PrefsUnifiedDlg::OnButtonEditAddr)
	EVT_BUTTON(ID_DESKTOPMODE, PrefsUnifiedDlg::OnButtonSystray)
	//EVT_BUTTON(IDC_WIZARD, PrefsUnifiedDlg::OnButtonWizard)
	EVT_BUTTON(IDC_IPFRELOAD, PrefsUnifiedDlg::OnButtonIPFilterReload)
	EVT_BUTTON(IDC_COLOR_BUTTON, PrefsUnifiedDlg::OnButtonColorChange)
	EVT_CHOICE(IDC_COLORSELECTOR, PrefsUnifiedDlg::OnColorCategorySelected)
	EVT_CHOICE(IDC_FCHECK, PrefsUnifiedDlg::OnFakeBrowserChange)
END_EVENT_TABLE()


PrefsUnifiedDlg::PrefsUnifiedDlg(wxWindow *parent)
	: wxDialog(parent,9990, _("Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxSYSTEM_MENU | wxRESIZE_BORDER)
{
	Rse *prse;
	int id;

	if (theApp.glob_prefs->BDlgTabsOnTop()) {
		preferencesDlgTop( this, TRUE ); 
	} else {
    		preferencesDlgLeft( this, TRUE ); 
	}
	
	CentreOnParent();
	pdtcShareSelector = ((CDirectoryTreeCtrl*)FindWindowById(IDC_SHARESELECTOR, this));
	pbuttonColor = (wxButton*)FindWindowById(IDC_COLOR_BUTTON, this);
	pchoiceColor = (wxChoice*)FindWindowById(IDC_COLORSELECTOR, this);

	// Link up the dlg items and listRse
	// First pass: get wxControl pointers and identify range of IDs
	idMax = 0;
	idMin = 0x7fffffff;
	
	wxListOfRseNode *pos;
	
	pos = listRse.GetFirst();
	while (pos) 	{		
		
		prse = pos->GetData();
		
		if ((id=prse->Id()) != 0) {
			if (id < idMin)
				idMin = id;
			if (id > idMax)
				idMax = id;
			// if the code crashes on the following line after you've added a new item to
			// listRse, then the ID used in the new list entry is probably wrong - wxWindows
			// does not seem to handle a non-existent ID gracefully inside FindWindowById().
			wxControl *pctrl = (wxControl *)FindWindowById(id, this);
			prse->SetWxControl(pctrl);
		}
		
		pos = pos->GetNext();
	}			
	wxASSERT(idMin<=idMax);
	// Second pass: build an array of pointers into listRse for fast access
	// [Note: the wxControl class lacks a "backpointer" facility - as a kludge
	// one could in principle use SetName / GetName with bent pointers, but that
	// would be very, very ugly and might break on future versions of wxWindows]
	trse = new Rse*[idMax-idMin+1];
	Rse** pprse;
	int i;
	for (pprse=trse, i=idMax-idMin+1;  i>0;  pprse++, i--) {
		*pprse = listRse.GetFirst()->GetData();  // dummy used for non-consecutive IDs
	}
	
	pos = listRse.GetFirst();
	while (pos) 	{		
		
		prse = pos->GetData();
	
		if ((id = prse->Id()) > 0)  {
			wxASSERT(id>=idMin && id<=idMax);
			trse[id-idMin] = prse;
		}
		pos = pos->GetNext();
	}
	
	// Third pass: establish links between items, e.g. dynamic labels of sliders
	pos = listRse.GetFirst();
	while (pos) 	{		
		
		prse = pos->GetData();
		if ((id = prse->IdLinkedTo()) != 0)
			Prse(id)->SetLink(prse);	
		pos = pos->GetNext();
	}
	
	wxASSERT(prseMaxUp == Prse(IDC_MAXUP));
	wxASSERT(prseMaxDown = Prse(IDC_MAXDOWN));
	
}



PrefsUnifiedDlg::~PrefsUnifiedDlg()
{
	delete[] trse;
	listRse.DeleteContents(true);
	listRse.Clear();
	
	/*		
	for (POSITION pos=listRse.GetHeadPosition();  pos!=NULL;  pos=listRse.NextAt(pos))
		delete listRse.GetAt(pos);
	listRse.RemoveAll();
	*/
}



Rse* PrefsUnifiedDlg::Prse(int id)	// returns the Rse* corresponding to an item ID
{ 
	wxASSERT(id>=idMin && id<=idMax);
	return trse[id-idMin]; 
}	


bool PrefsUnifiedDlg::Validate()
{
    return TRUE;
}



bool PrefsUnifiedDlg::TransferDataToWindow()
{	
	wxListOfRseNode *pos;
	
	pos = listRse.GetFirst();
	while (pos) 	{	
		(pos->GetData())->TransferToDlg();	
		pos = pos->GetNext();
	}		

	pdtcShareSelector->SetSharedDirectories(&theApp.glob_prefs->shareddir_list);
	CheckRateUnlimited(prseMaxUp);
	CheckRateUnlimited(prseMaxDown);
    return TRUE;
}



bool PrefsUnifiedDlg::TransferDataFromWindow()
{
	wxListOfRseNode *pos;
	pos = listRse.GetFirst();
	
	while (pos) 	{	
		(pos->GetData())->TransferFromDlg();
		pos = pos->GetNext();
	}		
	
	
	theApp.glob_prefs->shareddir_list.Clear();
	pdtcShareSelector->GetSharedDirectories(&theApp.glob_prefs->shareddir_list);
    return TRUE;
}


void PrefsUnifiedDlg::CheckRateUnlimited(Rse* prse)
{
	if (prse->GetMemValue() == 0)
		prse->SetMemValue(UNLIMITED);
	if (prse->GetMemValue() == UNLIMITED)
		prse->SetCtrlValue(0);
}



void PrefsUnifiedDlg::ForceUlDlRateCorrelation(int id)
// Here we slightly limit the users' ability to be a bad citizen: for very low upload rates
// we force a low download rate, so as to discourage this type of leeching.  
// We're Open Source, and whoever wants it can do his own mod to get around this, but the 
// packaged product will try to enforce good behavior. 
//
// Kry note: of course, any leecher mod will be banned asap.
//
// Also please note: Upload & Download Line "Capacities" are misnamed in the dlg right now: 
// as the eMule FAQ explains (and our code follows eMule in this respect), these values 
// are used only for scaling the statistics graphs and have no influence on network 
// I/O speeds, which are set set separately.  The old prefs limited the max line speeds to
// the values set as "capacities", the new prefs no longer do this for the following reason:
// My actual line capacity may be very high, say 256kB/s, but my DL rates may be much lower
// most of the time; if I want the graph to display with a useful scale, I will then set
// the "capacity" to the limit of actual experienced DL rates, say 60kB/s, yet I may not 
// want to limit my DLs to that rate, if I'm lucky enough to be sent data at a high rate 
// occasionally.  The confusion will go away when we move the spin controls for graph scaling 
// into the Statistics window, together with the sliders for update rate and averaging time.
{
	if (id == 0) {  // at init time no dlg exists yet, but check values loaded from file
		if (prseMaxUp->GetMemValue() < 4  && prseMaxUp->GetMemValue()*3 < prseMaxDown->GetMemValue())
			prseMaxDown->SetMemValue(prseMaxUp->GetMemValue()*3);
		else if (prseMaxUp->GetMemValue() < 10  && prseMaxUp->GetMemValue()*4 < prseMaxDown->GetMemValue())
			prseMaxDown->SetMemValue(prseMaxUp->GetMemValue()*4);
	} else {
		int kBpsUpMax = prseMaxUp->GetCtrlValue();
		int kBpsDownMax = prseMaxDown->GetCtrlValue();

		if (kBpsUpMax == 0  ||  kBpsUpMax >= 10) {
			prseMaxDown->SetCtrlRange(0, 19375 /* a magic number ?*/);
		} else if (kBpsUpMax < 4) {
			prseMaxDown->SetCtrlRange(1, kBpsUpMax*3);
			if (kBpsDownMax==0  ||  kBpsDownMax>kBpsUpMax*3)
				prseMaxDown->SetCtrlValue(kBpsUpMax*3);
		} else {
			prseMaxDown->SetCtrlRange(1, kBpsUpMax*4);
			if (kBpsDownMax==0  ||  kBpsDownMax>kBpsUpMax*4)
				prseMaxDown->SetCtrlValue(kBpsUpMax*4);
		}
	}
}



void PrefsUnifiedDlg::OnSpinMaxDLR(wxSpinEvent &event)
{
	ForceUlDlRateCorrelation(((wxControl*)event.GetEventObject())->GetId());
}



void PrefsUnifiedDlg::OnScroll(wxScrollEvent &event)
{
	wxControl*	pctrl = (wxControl*)event.GetEventObject();
	int			id = pctrl->GetId();
	Rse*		prse = Prse(id);

     prse->Propagate();

	
	// dynamic interactions
	switch (id) {
		case IDC_SLIDER:	prse->TransferFromDlg();
					 		theApp.amuledlg->statisticswnd->SetUpdatePeriod();	
							break;
		case IDC_SLIDER2:	prse->TransferFromDlg();	
							// tree update time needs no special function call
							break;
		case IDC_SLIDER3:	prse->TransferFromDlg();
							theApp.amuledlg->statisticswnd->ResetAveragingTime();
							break;
		case IDC_SLIDER4:	prse->TransferFromDlg();
							break;

		default:	break;
	}
}



void PrefsUnifiedDlg::OnOk(wxCommandEvent &event)
{
	TransferDataFromWindow();
	
	// do sanity checking, special processing, and user notifications here
	ForceUlDlRateCorrelation(0);
	CheckRateUnlimited(prseMaxUp);
	CheckRateUnlimited(prseMaxDown);
	Prse(IDC_UDPDISABLE)->SetCtrlValue(!Prse(IDC_UDPPORT)->GetMemValue());
	Prse(IDC_FCHECKSELF)->SetEnabled(Prse(IDC_FCHECK)->GetMemValue()==(Prse(IDC_FCHECK)->GetCtrlCount() -1));
	// save the preferences on ok
	if (theApp.glob_prefs) {
		theApp.glob_prefs->Save(); }
	
	if (Prse(IDC_FED2KLH)->WasChanged()) 
		theApp.amuledlg->ToggleFastED2KLinksHandler();
	if (Prse(IDC_LANGUAGE)->WasChanged())
		wxMessageBox(wxString::wxString(_("Language change will not be applied until aMule is restarted.")));

	if (Prse(IDC_INCFILES)->WasChanged() || Prse(IDC_TEMPFILES)->WasChanged() || pdtcShareSelector->HasChanged) {
		theApp.sharedfiles->Reload(true, false);
	}
	
	if (Prse(IDC_PERCENT)->WasChanged() || Prse(IDC_PROGBAR)->WasChanged()) {		
		// Force upload of the donwload queue 
		theApp.downloadqueue->UpdateDisplayedInfo(true);
	}

	if (Prse(IDC_OSDIR)->WasChanged()) {		
		// Build the filenames for the two OS files
		theApp.SetOSFiles(Prse(IDC_OSDIR)->GetMemStringValue());
	}
	
    EndModal(ID_PREFS_OK_LEFT);
}


void PrefsUnifiedDlg::OnCancel(wxCommandEvent &event)
{
	// undo interactive changes here, e.g. stats graphs parameter settings
	Prse(IDC_SLIDER)->RestorePrevValue();
	Prse(IDC_SLIDER3)->RestorePrevValue();
	Prse(IDC_SLIDER2)->RestorePrevValue();
	Prse(IDC_SLIDER4)->RestorePrevValue();
	Prse(IDC_COLOR_BUTTON)->RestorePrevValue();
	theApp.amuledlg->statisticswnd->SetUpdatePeriod();	
	theApp.amuledlg->statisticswnd->ResetAveragingTime();
	for (int i=0; i<cntStatColors; i++) {
		aprseColor[i]->RestorePrevValue();
		theApp.amuledlg->statisticswnd->ApplyStatsColor(i);
	}
	
    EndModal(ID_PREFS_CANCEL_LEFT);
}



void PrefsUnifiedDlg::OnCheckBoxChange(wxCommandEvent& event)
{
	wxCheckBox*	pbox = (wxCheckBox*)event.GetEventObject();
	int			id = pbox->GetId();
	bool		bIsChecked = pbox->GetValue();

	switch (id) {
		case IDC_UDPDISABLE: {
			Rse* prse = Prse(IDC_UDPPORT);
			if (bIsChecked) {
				prse->SetCtrlRange(0,0);
				prse->SetCtrlValue(0);
			} else {
				prse->SetCtrlRange(1025,65535);
				int iPrev = prse->GetPrevValue();
				prse->SetCtrlValue(iPrev ? iPrev : Prse(IDC_PORT)->GetCtrlValue()+10);
			}
			break;
		}
				
		default:	break;
	}
}

void PrefsUnifiedDlg::OnButtonColorChange(wxCommandEvent &event)
{
	int index = GetColorIndex();
	wxColour col = WxColourFromCr(aprseColor[index]->GetMemValue());
	col = wxGetColourFromUser(this, col);
	if(col.Ok()) {
		pbuttonColor->SetBackgroundColour(col);
		aprseColor[index]->SetMemValue(CrFromWxColour(col));
		theApp.amuledlg->statisticswnd->ApplyStatsColor(index);
	}
}


void PrefsUnifiedDlg::OnColorCategorySelected(wxCommandEvent& evt)
{
	pbuttonColor->SetBackgroundColour(WxColourFromCr(aprseColor[GetColorIndex()]->GetMemValue()));
}


void PrefsUnifiedDlg::OnFakeBrowserChange(wxCommandEvent& evt)
{
	Prse(IDC_FCHECK)->StoreDlgValue();
	Prse(IDC_FCHECKSELF)->SetEnabled(Prse(IDC_FCHECK)->GetMemValue()==(Prse(IDC_FCHECK)->GetCtrlCount() -1));
}

void PrefsUnifiedDlg::OnButtonSystray(wxCommandEvent& evt)
{
	theApp.amuledlg->changeDesktopMode();
}



void PrefsUnifiedDlg::OnButtonDir(wxCommandEvent& event)
{
	wxControl*	pctrl = (wxControl*)event.GetEventObject();
	int			idButton = pctrl->GetId();
	Rse*		prse;
	
	if (idButton == IDC_SELTEMPDIR)
		prse = Prse(IDC_TEMPFILES);
	else if (idButton == IDC_SELINCDIR)
		prse = Prse(IDC_INCFILES);
	else if (idButton == IDC_SELOSDIR)
		prse = Prse(IDC_OSDIR);
	else
		wxASSERT(false);
	((RseDirAssured*)prse)->SelectDir();
}



void PrefsUnifiedDlg::OnButtonBrowseWav(wxCommandEvent& evt)
{
	wxString str = wxFileSelector(_("Browse wav"),wxT(""),wxT(""),wxT("*.wav"),_("File wav (*.wav)|*.wav||"));
	Prse(IDC_EDIT_TBN_WAVFILE)->SetCtrlValue(str);
}



void PrefsUnifiedDlg::OnButtonBrowseVideoplayer(wxCommandEvent& e)
{
	wxString str=wxFileSelector(_("Browse for videoplayer"),wxT(""),wxT(""),wxT(""),_("Executable (*)|*||"));
	if(!str.IsEmpty()) 
		Prse(IDC_VIDEOPLAYER)->SetCtrlValue(str);
}



void PrefsUnifiedDlg::OnButtonEditAddr(wxCommandEvent& evt)
{
	wxString fullpath(theApp.ConfigDir + wxT("addresses.dat"));
	
	EditServerListDlg* test=new EditServerListDlg(this, _("Edit Serverlist"),
	_("Add here URL's to download server.met files.\nOnly one url on each line."), fullpath);
	test->ShowModal();
  
	delete test;
}

void PrefsUnifiedDlg::OnButtonIPFilterReload(wxCommandEvent &event) {
	theApp.ipfilter->Reload();
}	
void PrefsUnifiedDlg::LoadAllItems(wxConfigBase& ini)
{
	wxListOfRseNode *pos;
	
	pos = listRse.GetFirst();
	while (pos) 	{	
		(pos->GetData())->LoadFromFile(ini);
	
		pos = pos->GetNext();
	}
	
	// Now do some post-processing / sanity checking on the values we just loaded
	ForceUlDlRateCorrelation(0);
	
}



void PrefsUnifiedDlg::SaveAllItems(wxConfigBase& ini)
{
	wxListOfRseNode *pos;
	
	pos = listRse.GetFirst();
	while (pos) 	{	
		(pos->GetData())->SaveToFile(ini);
		pos = pos->GetNext();
	}
}
