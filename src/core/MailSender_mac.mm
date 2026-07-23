#include "MailSender.h"

#import <AppKit/AppKit.h>

// macOS equivalent of the Windows MAPI path (MailSender.cpp): hands off to
// the OS's own mail-compose sheet (NSSharingService), pre-filled with the
// recipient/subject/body/attachment, and lets the user review and click
// Send themselves — this never transmits anything on its own, same contract
// as the Windows implementation.
namespace MailSender {

bool sendWithAttachment(const QString &toName, const QString &toAddress, const QString &subject,
                         const QString &body, const QString &attachmentPath, QString *errorMessage)
{
    Q_UNUSED(toName); // NSSharingService only takes an address, no separate display name

    @autoreleasepool {
        NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameComposeEmail];
        if (!service) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Aucune application de messagerie configurée sur ce Mac (app Mail requise).");
            }
            return false;
        }

        service.recipients = @[ toAddress.toNSString() ];
        service.subject = subject.toNSString();

        NSURL *fileURL = [NSURL fileURLWithPath:attachmentPath.toNSString()];
        NSMutableArray *items = [NSMutableArray array];
        if (!body.isEmpty())
            [items addObject:body.toNSString()];
        [items addObject:fileURL];

        if (![service canPerformWithItems:items]) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Impossible d'ouvrir un brouillon de courriel (application Mail non configurée ?).");
            }
            return false;
        }

        // Shows the Mail compose sheet pre-filled; the person still has to
        // review and click Send themselves.
        [service performWithItems:items];
        return true;
    }
}

} // namespace MailSender
