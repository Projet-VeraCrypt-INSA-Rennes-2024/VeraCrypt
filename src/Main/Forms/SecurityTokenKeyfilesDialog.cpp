/*
 Derived from source code of TrueCrypt 7.1a, which is
 Copyright (c) 2008-2012 TrueCrypt Developers Association and which is governed
 by the TrueCrypt License 3.0.

 Modifications and additions to the original source code (contained in this file)
 and all other portions of this file are Copyright (c) 2013-2017 IDRIX
 and are governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/

#include "System.h"
#include "Main/GraphicUserInterface.h"
#include "NewSecurityTokenKeyfileDialog.h"
#include "SecurityTokenKeyfilesDialog.h"
#include "PKCS11/pkcs11t.h"

namespace VeraCrypt
{
	SecurityTokenKeyfilesDialog::SecurityTokenKeyfilesDialog (wxWindow* parent, bool selectionMode, KeyDisplay displayMode)
		: SecurityTokenKeyfilesDialogBase (parent),
        keyDisplayMode(displayMode)
	{
		if (selectionMode)
			SetTitle (LangString["SELECT_TOKEN_KEYFILES"]);

		list <int> colPermilles;

		SecurityTokenKeyfileListCtrl->InsertColumn (ColumnSecurityTokenSlotId, LangString["TOKEN_SLOT_ID"], wxLIST_FORMAT_CENTER, 1);
		colPermilles.push_back (102);
		SecurityTokenKeyfileListCtrl->InsertColumn (ColumnSecurityTokenLabel, LangString["TOKEN_NAME"], wxLIST_FORMAT_LEFT, 1);
		colPermilles.push_back (368);
		SecurityTokenKeyfileListCtrl->InsertColumn (ColumnSecurityTokenKeyfileLabel, LangString["TOKEN_DATA_OBJECT_LABEL"], wxLIST_FORMAT_LEFT, 1);
		colPermilles.push_back (529);

        //TODO: i18n
        SecurityTokenCertificateListCtrl->InsertColumn(ColumnSecurityTokenCertificate, "Clé", wxLIST_FORMAT_LEFT, 1);
        SecurityTokenCertificateListCtrl->SetSingleStyle(wxLC_SINGLE_SEL, true);

		FillSecurityTokenKeyfileListCtrl();

        switch (displayMode)
        {
            case KeyDisplay::PrivateKeysOnly:
                std::cout << "private only\n";
                break;
            case KeyDisplay::PublicKeysOnly:
                std::cout << "public only\n";
                break;
            case KeyDisplay::Both:
                std::cout << "both\n";
                break;
            case KeyDisplay::None:
                std::cout << "none\n";
                break;
        }

        if(displayMode != KeyDisplay::None)
        {
            FillSecurityTokenCertificateListCtrl();
        }

		Gui->SetListCtrlWidth (SecurityTokenKeyfileListCtrl, 65);
		Gui->SetListCtrlHeight (SecurityTokenKeyfileListCtrl, 16);
		Gui->SetListCtrlColumnWidths (SecurityTokenKeyfileListCtrl, colPermilles);

        //TODO: ajuster la taille en fonction du plus grand label existant dans la liste
        SecurityTokenCertificateListCtrl->SetColumnWidth(ColumnSecurityTokenCertificate, 500);

		Fit();
		Layout();
		Center();

		DeleteButton->Disable();
		ExportButton->Disable();
		OKButton->Disable();
		OKButton->SetDefault();
	}

	void SecurityTokenKeyfilesDialog::FillSecurityTokenKeyfileListCtrl ()
	{
		wxBusyCursor busy;

		SecurityTokenKeyfileListCtrl->DeleteAllItems();
		SecurityTokenKeyfileList = Token::GetAvailableKeyfiles(Gui->GetPreferences().EMVSupportEnabled);

		foreach (const shared_ptr<TokenKeyfile> key, SecurityTokenKeyfileList)
		{
			vector <wstring> fields (SecurityTokenKeyfileListCtrl->GetColumnCount());

			fields[ColumnSecurityTokenSlotId] = StringConverter::ToWide ((uint64) key->Token->SlotId);
			fields[ColumnSecurityTokenLabel] = key->Token->Label;
			fields[ColumnSecurityTokenKeyfileLabel] = key->Id;

			Gui->AppendToListCtrl (SecurityTokenKeyfileListCtrl, fields, 0, key.get());
		}
	}

    void SecurityTokenKeyfilesDialog::FillSecurityTokenCertificateListCtrl ()
    {
        //TODO: meilleur code que ça
		/*CK_SLOT_ID slotId = SecurityToken::GetTokenSlots().front();

        SecurityTokenCertificateListCtrl->DeleteAllItems();
        //TODO: faire la distinction entre privé et public
        SecurityTokenCertificateList = SecurityToken::GetKeyFromPkcs11(CKO_PRIVATE_KEY);

        //TODO: pour toutes les sessions
        foreach(const CK_OBJECT_HANDLE handle, SecurityTokenCertificateList)
        {
            vector<byte> labelAsBytes;
            SecurityToken::GetObjectAttribute(slotId, handle, CKA_LABEL, labelAsBytes);
            string labelAsText = string(reinterpret_cast<const char*>(labelAsBytes.data()), labelAsBytes.size());

            wxListItem keyItem;
            keyItem->setData(reinterpret_cast<void*>(handle));
            keyItem->setText(labelAsText);

            SecurityTokenCertificateListCtrl->InsertItem(keyItem);
        }*/

        SecurityTokenCertificateListCtrl->DeleteAllItems();

        for(int i = 0; i < 10; i++)
        {
            wxListItem keyItem;
            keyItem.SetId(i);
            keyItem.SetData((void*)(i*20));
            keyItem.SetText(to_string(i));
            SecurityTokenCertificateListCtrl->InsertItem(keyItem);
        }
    }

	void SecurityTokenKeyfilesDialog::OnDeleteButtonClick (wxCommandEvent& event)
	{
		try
		{
			if (!Gui->AskYesNo (LangString["CONFIRM_SEL_FILES_DELETE"]))
				return;

			wxBusyCursor busy;

			foreach (long item, Gui->GetListCtrlSelectedItems (SecurityTokenKeyfileListCtrl))
			{
				SecurityToken::DeleteKeyfile (*reinterpret_cast <SecurityTokenKeyfile *> (SecurityTokenKeyfileListCtrl->GetItemData (item)));
			}

			FillSecurityTokenKeyfileListCtrl();
		}
		catch (exception &e)
		{
			Gui->ShowError (e);
		}
	}

	void SecurityTokenKeyfilesDialog::OnExportButtonClick (wxCommandEvent& event)
	{
		try
		{
			foreach (long item, Gui->GetListCtrlSelectedItems (SecurityTokenKeyfileListCtrl))
			{
				TokenKeyfile *keyfile = reinterpret_cast <TokenKeyfile *> (SecurityTokenKeyfileListCtrl->GetItemData (item));

				FilePathList files = Gui->SelectFiles (this, wxEmptyString, true);

				if (!files.empty())
				{
					wxBusyCursor busy;

					vector <byte> keyfileData;
					keyfile->GetKeyfileData (keyfileData);

					BufferPtr keyfileDataBuf (&keyfileData.front(), keyfileData.size());
					finally_do_arg (BufferPtr, keyfileDataBuf, { finally_arg.Erase(); });

					File keyfile;
					keyfile.Open (*files.front(), File::CreateWrite);
					keyfile.Write (keyfileDataBuf);
				}
				else
					break;

				Gui->ShowInfo ("KEYFILE_EXPORTED");
			}
		}
		catch (exception &e)
		{
			Gui->ShowError (e);
		}
	}

	void SecurityTokenKeyfilesDialog::OnImportButtonClick (wxCommandEvent& event)
	{
		try
		{
			FilePathList keyfilePaths = Gui->SelectFiles (this, LangString["SELECT_KEYFILES"], false);

			if (keyfilePaths.empty())
				return;

			FilePath keyfilePath = *keyfilePaths.front();

			File keyfile;
			keyfile.Open (keyfilePath, File::OpenRead, File::ShareReadWrite, File::PreserveTimestamps);

			if (keyfile.Length() > 0)
			{
				vector <byte> keyfileData (keyfile.Length());
				BufferPtr keyfileDataBuf (&keyfileData.front(), keyfileData.size());

				keyfile.ReadCompleteBuffer (keyfileDataBuf);
				finally_do_arg (BufferPtr, keyfileDataBuf, { finally_arg.Erase(); });

				NewSecurityTokenKeyfileDialog newKeyfileDialog (this, keyfilePath.ToBaseName());

				if (newKeyfileDialog.ShowModal() == wxID_OK)
				{
					wxBusyCursor busy;
					SecurityToken::CreateKeyfile (newKeyfileDialog.GetSelectedSlotId(), keyfileData, StringConverter::ToSingle (newKeyfileDialog.GetKeyfileName()));

					FillSecurityTokenKeyfileListCtrl();
				}
			}
			else
				throw InsufficientData (SRC_POS, keyfilePath);
		}
		catch (exception &e)
		{
			Gui->ShowError (e);
		}
	}

	void SecurityTokenKeyfilesDialog::OnListItemDeselected (wxListEvent& event)
	{
		if (SecurityTokenKeyfileListCtrl->GetSelectedItemCount() == 0 && SecurityTokenCertificateListCtrl->GetSelectedItemCount() == 0)
		{
			DeleteButton->Disable();
			ExportButton->Disable();
			OKButton->Disable();
		}
	}

	void SecurityTokenKeyfilesDialog::OnListItemSelected(wxListEvent &event)
	{
		if (event.GetItem().GetData() != (wxUIntPtr) nullptr)
		{
			BOOL deletable = true;
			foreach(long
			item, Gui->GetListCtrlSelectedItems(SecurityTokenKeyfileListCtrl))
			{
				TokenKeyfile *keyfile = reinterpret_cast <TokenKeyfile *> (SecurityTokenKeyfileListCtrl->GetItemData(item));
				if (!keyfile->Token->isEditable())
				{
					deletable = false;
					break;
				}
			}
			if (deletable)
			{
				DeleteButton->Enable();
			}
			ExportButton->Enable();
			OKButton->Enable();
		}
	}

    void SecurityTokenKeyfilesDialog::OnKeyListItemSelected(wxListEvent& event)
    {
        OKButton->Enable();
    }

	void SecurityTokenKeyfilesDialog::OnOKButtonClick ()
	{
		foreach (long item, Gui->GetListCtrlSelectedItems (SecurityTokenKeyfileListCtrl))
		{
			TokenKeyfile *key = reinterpret_cast <TokenKeyfile *> (SecurityTokenKeyfileListCtrl->GetItemData(item));

			SelectedSecurityTokenKeyfilePaths.push_back(*key);
		}

        long selectedItemId = SecurityTokenCertificateListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedItemId == -1)
        {
            return;
        }

        int itemData = (int)SecurityTokenCertificateListCtrl->GetItemData(selectedItemId);
        cout << "Selected key data: " << itemData << "\n";

		EndModal (wxID_OK);
	}
}
