#include "Ios/Title/TicketView.h"

namespace Ios
{

TicketView MakeTicketView(const Formats::Ticket& ticket)
{
    TicketView view;
    view.formatVersion = ticket.formatVersion;
    view.ticketId = ticket.ticketId;
    view.consoleId = ticket.consoleId;
    view.titleId = TitleId::FromValue(ticket.titleId);
    view.accessMask = ticket.accessMask;
    view.titleVersion = ticket.titleVersion;
    view.permittedTitlesMask = ticket.permittedTitlesMask;
    view.permitMask = ticket.permitMask;
    view.isTitleExportAllowed = ticket.isTitleExportAllowed;
    view.reserved = ticket.reserved;
    view.contentAccess = ticket.contentAccess;
    view.limits = ticket.limits;
    return view;
}

}
