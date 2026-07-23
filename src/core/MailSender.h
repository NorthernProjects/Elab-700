#pragma once

#include <QString>

// Opens the user's default mail client (via Windows Simple MAPI) with the
// recipient, subject and attachment pre-filled, and shows its compose
// window so a person reviews and clicks Send themselves — this never sends
// mail silently on its own.
namespace MailSender {

bool sendWithAttachment(const QString &toName, const QString &toAddress, const QString &subject,
                         const QString &body, const QString &attachmentPath, QString *errorMessage);

} // namespace MailSender
