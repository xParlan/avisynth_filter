#include "pch.h"
#include "prop_settings.h"
#include "api.h"
#include "avs_handler.h"
#include "constants.h"
#include "environment.h"
#include "util.h"
#include "version.h"


namespace AvsFilter {

CAvsFilterPropSettings::CAvsFilterPropSettings(LPUNKNOWN pUnk, HRESULT *phr)
    : CBasePropertyPage(NAME(SETTINGS_FULL), pUnk, IDD_SETTINGS_PAGE, IDS_SETTINGS)
    , _filter(nullptr)
    , _avsFileManagedByRC(false) {
}

auto CAvsFilterPropSettings::OnConnect(IUnknown *pUnk) -> HRESULT {
    CheckPointer(pUnk, E_POINTER);
    return pUnk->QueryInterface(IID_IAvsFilter, reinterpret_cast<void **>(&_filter));
}

auto CAvsFilterPropSettings::OnDisconnect() -> HRESULT {
    if (_filter != nullptr) {
        _filter->Release();
        _filter = nullptr;
    }

    return S_OK;
}

auto CAvsFilterPropSettings::OnActivate() -> HRESULT {
    _configAvsFile = g_env->GetAvsFile();
    _avsFileManagedByRC = _configAvsFile != _filter->GetEffectiveAvsFile();
    if (_avsFileManagedByRC) {
        ShowWindow(GetDlgItem(m_Dlg, IDC_TEXT_RC_CONTROLLING), SW_SHOW);
    }

    SetDlgItemText(m_Dlg, IDC_EDIT_AVS_FILE, _configAvsFile.c_str());

    EnableWindow(GetDlgItem(m_Dlg, IDC_BUTTON_RELOAD), !_avsFileManagedByRC && _filter->GetAvsState() != AvsState::Stopped);

    const DWORD formatBits = g_env->GetInputFormatBits();
    for (int i = 0; i < IDC_INPUT_FORMAT_END - IDC_INPUT_FORMAT_START; ++i) {
        if ((formatBits & (1 << i)) != 0) {
            CheckDlgButton(m_Dlg, IDC_INPUT_FORMAT_START + 1 + i, 1);
        }
    }

    const std::wstring title = std::wstring(L"<a>") + Widen(FILTER_NAME_BASE) +
        L" v" + Widen(FILTER_VERSION_STRING) +
        L"</a> with " + ConvertUtf8ToWide(g_avs->GetVersionString());
    SetDlgItemText(m_hwnd, IDC_SYSLINK_TITLE, title.c_str());

    // move the focus to the tab of the settings page, effectively unfocus all controls in the page
    PostMessage(m_hwnd, WM_NEXTDLGCTL, 1, FALSE);

    return S_OK;
}

auto CAvsFilterPropSettings::OnApplyChanges() -> HRESULT {
    g_env->SetAvsFile(_configAvsFile);

    DWORD formatBits = 0;
    for (int i = 0; i < IDC_INPUT_FORMAT_END - IDC_INPUT_FORMAT_START; ++i) {
        if (IsDlgButtonChecked(m_Dlg, IDC_INPUT_FORMAT_START + 1 + i) == BST_CHECKED) {
            formatBits |= 1 << i;
        }
    }
    g_env->SetInputFormatBits(formatBits);

    g_env->SaveConfig();

    if (_avsFileManagedByRC) {
        // TODO: put message in string table when going multi-language
        MessageBox(m_hwnd, L"AviSynth script file is currently managed by remote control. Your change if any is saved but not used.",
                   FILTER_NAME_WIDE, MB_OK | MB_ICONINFORMATION);
    } else if (!_configAvsFile.empty()) {
        _filter->ReloadAvsFile(_configAvsFile);
    }

    return S_OK;
}

auto CAvsFilterPropSettings::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> INT_PTR {
    switch (uMsg) {
    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE) {
            const WORD eventTarget = LOWORD(wParam);

            if (eventTarget == IDC_EDIT_AVS_FILE) {
                wchar_t buf[STR_MAX_LENGTH];
                GetDlgItemText(hwnd, IDC_EDIT_AVS_FILE, buf, STR_MAX_LENGTH);
                const std::wstring newValue = std::wstring(buf, STR_MAX_LENGTH).c_str();

                if (newValue != _configAvsFile) {
                    _configAvsFile = newValue;
                    SetDirty();
                }

                return 0;
            }
        } else if (HIWORD(wParam) == BN_CLICKED) {
            const WORD eventTarget = LOWORD(wParam);

            if (eventTarget == IDC_BUTTON_EDIT && !_configAvsFile.empty()) {
                ShellExecute(hwnd, L"edit", _configAvsFile.c_str(), nullptr, nullptr, SW_SHOW);
            } else if (eventTarget == IDC_BUTTON_RELOAD) {
                _filter->ReloadAvsFile(_filter->GetEffectiveAvsFile());
            } else if (eventTarget == IDC_BUTTON_BROWSE) {
                wchar_t szFile[MAX_PATH] {};

                OPENFILENAME ofn {};
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = L"avs Files\0*.avs\0All Files\0*.*\0";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                if (GetOpenFileName(&ofn) == TRUE) {
                    SetDlgItemText(hwnd, IDC_EDIT_AVS_FILE, ofn.lpstrFile);
                    SetDirty();
                }
            } else if (eventTarget > IDC_INPUT_FORMAT_START && eventTarget < IDC_INPUT_FORMAT_END) {
                SetDirty();
            }

            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
        if (GetWindowLong(reinterpret_cast<HWND>(lParam), GWL_ID) == IDC_TEXT_RC_CONTROLLING) {
            const HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            // make the color of the text control (IDC_TEXT_RC_CONTROLLING) blue to catch attention
            SetTextColor(hdc, RGB(0, 0, 0xff));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        break;

    case WM_NOTIFY: {
        const LPNMHDR notifyHeader = reinterpret_cast<LPNMHDR>(lParam);

        if (notifyHeader->idFrom == IDC_SYSLINK_TITLE) {
            switch (notifyHeader->code) {
            case NM_CLICK:
            case NM_RETURN:
                ShellExecute(hwnd, L"open", L"https://github.com/CrendKing/avisynth_filter", nullptr, nullptr, SW_SHOW);
                return 0;
            }
        }

        break;
    }
    }

    return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

void CAvsFilterPropSettings::SetDirty() {
    m_bDirty = TRUE;
    if (m_pPageSite) {
        m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }
}

}
