#include "MailSender.h"

#include <windows.h>

#include <mapi.h>

#include <QDir>
#include <QFileInfo>

namespace MailSender {

namespace {
// Simple MAPI has no import library in current Windows SDKs (link-time
// MAPISendMail fails), so mapi32.dll is loaded dynamically at runtime
// instead — same approach as the camera backend's toupcam.dll loading.
using FnMapiSendMail = ULONG(WINAPI *)(LHANDLE, ULONG_PTR, lpMapiMessage, FLONG, ULONG);
}

bool sendWithAttachment(const QString &toName, const QString &toAddress, const QString &subject,
                         const QString &body, const QString &attachmentPath, QString *errorMessage)
{
    HMODULE mapiModule = LoadLibraryW(L"mapi32.dll");
    if (!mapiModule) {
        if (errorMessage)
            *errorMessage = QStringLiteral("mapi32.dll introuvable : aucun client de messagerie MAPI n'est installé.");
        return false;
    }

    auto mapiSendMail = reinterpret_cast<FnMapiSendMail>(GetProcAddress(mapiModule, "MAPISendMail"));
    if (!mapiSendMail) {
        FreeLibrary(mapiModule);
        if (errorMessage)
            *errorMessage = QStringLiteral("Fonction MAPISendMail introuvable dans mapi32.dll.");
        return false;
    }

    // Both lpszName (display name) and lpszAddress (the actual resolvable
    // address) need to be set — leaving lpszAddress empty (the previous bug
    // here) made some MAPI clients fail to resolve the recipient at all
    // instead of just showing the raw address as the display name.
    const QByteArray toNameBytes = (toName.isEmpty() ? toAddress : toName).toLocal8Bit();
    const QByteArray toAddressBytes = (QStringLiteral("SMTP:") + toAddress).toLocal8Bit();
    const QByteArray subjectBytes = subject.toLocal8Bit();
    const QByteArray bodyBytes = body.toLocal8Bit();
    const QByteArray pathBytes = QDir::toNativeSeparators(attachmentPath).toLocal8Bit();
    const QByteArray fileNameBytes = QFileInfo(attachmentPath).fileName().toLocal8Bit();

    MapiRecipDesc recipient = {};
    recipient.ulRecipClass = MAPI_TO;
    recipient.lpszName = const_cast<char *>(toNameBytes.constData());
    recipient.lpszAddress = const_cast<char *>(toAddressBytes.constData());

    MapiFileDesc fileDesc = {};
    fileDesc.nPosition = static_cast<ULONG>(-1);
    fileDesc.lpszPathName = const_cast<char *>(pathBytes.constData());
    fileDesc.lpszFileName = const_cast<char *>(fileNameBytes.constData());

    MapiMessage message = {};
    message.lpszSubject = const_cast<char *>(subjectBytes.constData());
    message.lpszNoteText = const_cast<char *>(bodyBytes.constData());
    message.nRecipCount = 1;
    message.lpRecips = &recipient;
    message.nFileCount = 1;
    message.lpFiles = &fileDesc;

    // MAPI_DIALOG shows the mail client's own compose window pre-filled with
    // the above, so the person still has to review and click Send
    // themselves — this function never transmits anything on its own.
    const ULONG result = mapiSendMail(0, 0, &message, MAPI_DIALOG | MAPI_LOGON_UI, 0);
    FreeLibrary(mapiModule);

    if (result != SUCCESS_SUCCESS && result != MAPI_USER_ABORT) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Aucun client de messagerie MAPI n'a pu être ouvert (code %1). "
                "Vérifiez qu'un client de messagerie (Outlook, etc.) est installé et configuré par défaut.")
                .arg(result);
        }
        return false;
    }
    return true;
}

} // namespace MailSender
