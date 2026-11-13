/*
 * tde-nmctl - NetworkManager front-end for the Trinity Desktop Environment
 *
 * Copyright (C) 2026 tde-nmctl developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "connectionlist.h"
#include "connectiondialog.h"
#include "authorization.h"
#include "nmclient.h"
#include "nmtext.h"
#include "secretstore.h"

#include <tqdatetime.h>
#include <tqfile.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>

#include <tdefiledialog.h>
#include <kiconloader.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <tdepopupmenu.h>
#include <kprocio.h>
#include <kstandarddirs.h>

enum { ColName = 0, ColState, ColUsed, ColPath };

ConnectionList::ConnectionList(TQWidget *parent, const char *name)
	: TQWidget(parent, name)
{
	TQVBoxLayout *top = new TQVBoxLayout(this, 0, 6);

	TQHBoxLayout *auth = new TQHBoxLayout(top, 6);
	m_authIcon = new TQLabel(this);
	m_authIcon->setPixmap(SmallIcon("encrypted"));
	m_authText = new TQLabel(this);
	m_authText->setAlignment(TQt::WordBreak | TQt::AlignVCenter);
	auth->addWidget(m_authIcon);
	auth->addWidget(m_authText, 1);

	TQHBoxLayout *main = new TQHBoxLayout(top, 6);
	m_list = new TDEListView(this);
	m_list->addColumn(i18n("Name"));
	m_list->addColumn(i18n("State"));
	m_list->addColumn(i18n("Last Used"));
	m_list->setAllColumnsShowFocus(true);
	m_list->setRootIsDecorated(true);
	m_list->setSorting(0);
	m_list->setResizeMode(TQListView::NoColumn);
	m_list->setColumnWidth(0, 280);
	m_list->setColumnWidth(1, 120);
	m_list->setColumnWidth(2, 160);
	m_list->setMinimumSize(480, 300);
	main->addWidget(m_list, 1);

	TQVBoxLayout *buttons = new TQVBoxLayout(main, 6);
	m_add = new TQPushButton(SmallIconSet("add"), i18n("&Add"), this);
	m_edit = new TQPushButton(SmallIconSet("edit"), i18n("&Edit..."), this);
	m_delete = new TQPushButton(SmallIconSet("edit-delete"), i18n("&Delete"), this);
	m_duplicate = new TQPushButton(SmallIconSet("edit-copy"), i18n("D&uplicate"), this);
	m_connect = new TQPushButton(SmallIconSet("connect_established"), i18n("&Connect"), this);
	m_import = new TQPushButton(SmallIconSet("document-import"), i18n("&Import VPN..."), this);
	m_export = new TQPushButton(SmallIconSet("document-export"), i18n("E&xport..."), this);
	buttons->addWidget(m_add);
	buttons->addWidget(m_edit);
	buttons->addWidget(m_delete);
	buttons->addWidget(m_duplicate);
	buttons->addSpacing(12);
	buttons->addWidget(m_connect);
	buttons->addSpacing(12);
	buttons->addWidget(m_import);
	buttons->addWidget(m_export);
	buttons->addStretch(1);

	m_addMenu = new TDEPopupMenu(this);
	m_addTypes = NMText::creatableTypes();
	m_addMenu->insertTitle(i18n("New Connection"));
	TQString group;
	for (uint i = 0; i < m_addTypes.count(); ++i) {
		TQString g = NMText::connectionGroup(m_addTypes[i]);
		if (!group.isEmpty() && g != group)
			m_addMenu->insertSeparator();
		group = g;
		m_addMenu->insertItem(SmallIcon(NMText::connectionIcon(m_addTypes[i])),
		                      NMText::connectionType(m_addTypes[i]), i);
	}
	m_addMenu->insertSeparator();
	m_addMenu->insertItem(SmallIcon("tde-nmctl-signal-100"), i18n("Wi-Fi Hotspot"), 1000);
	m_add->setPopup(m_addMenu);
	connect(m_addMenu, TQ_SIGNAL(activated(int)), TQ_SLOT(slotAddMenu(int)));

	connect(m_edit, TQ_SIGNAL(clicked()), TQ_SLOT(editSelected()));
	connect(m_delete, TQ_SIGNAL(clicked()), TQ_SLOT(deleteSelected()));
	connect(m_duplicate, TQ_SIGNAL(clicked()), TQ_SLOT(duplicateSelected()));
	connect(m_connect, TQ_SIGNAL(clicked()), TQ_SLOT(toggleSelected()));
	connect(m_import, TQ_SIGNAL(clicked()), TQ_SLOT(importVpn()));
	connect(m_export, TQ_SIGNAL(clicked()), TQ_SLOT(exportSelected()));
	connect(m_list, TQ_SIGNAL(selectionChanged()), TQ_SLOT(slotSelectionChanged()));
	connect(m_list, TQ_SIGNAL(executed(TQListViewItem*)), TQ_SLOT(editSelected()));
	connect(m_list, TQ_SIGNAL(contextMenuRequested(TQListViewItem*, const TQPoint&, int)),
	        TQ_SLOT(slotContextMenu(TQListViewItem*, const TQPoint&, int)));

	NMClient *nm = NMClient::self();
	connect(nm, TQ_SIGNAL(connectionsChanged()), TQ_SLOT(reload()));
	connect(nm, TQ_SIGNAL(changed()), TQ_SLOT(updateStates()));
	connect(nm, TQ_SIGNAL(operationFinished(bool, const TQString&, const TQString&)),
	        TQ_SLOT(slotOperationFinished(bool, const TQString&, const TQString&)));

	updateAuthorization();
	reload();
}

void ConnectionList::updateAuthorization()
{
	NMAuthorization::Result r = NMAuthorization::check(NM_AUTH_MODIFY_SYSTEM);
	TQString text;
	if (!NMClient::self()->isRunning())
		text = i18n("<qt><b>NetworkManager is not running.</b> Start the NetworkManager service "
		            "to manage network connections.</qt>");
	else
		text = NMAuthorization::explain(r);
	m_authText->setText(text);
	m_authIcon->setShown(!text.isEmpty());
	m_authText->setShown(!text.isEmpty());
}

static TQString lastUsed(TQ_UINT64 ts)
{
	if (ts == 0)
		return i18n("never");
	TQDateTime t;
	t.setTime_t((uint)ts);
	int days = t.daysTo(TQDateTime::currentDateTime());
	if (days == 0)
		return i18n("today, %1").arg(TDEGlobal::locale()->formatTime(t.time()));
	if (days == 1)
		return i18n("yesterday");
	if (days < 30)
		return i18n("%n day ago", "%n days ago", days);
	return TDEGlobal::locale()->formatDate(t.date(), true);
}

void ConnectionList::reload()
{
	TQString selected = selectedPath();
	TQMap<TQString, TDEListViewItem *> groups;
	m_list->clear();

	const NMConnectionList &all = NMClient::self()->connections();
	for (NMConnectionList::const_iterator it = all.begin(); it != all.end(); ++it) {
		TQString type = (*it).type();
		if (type == "loopback" || type == "generic")
			continue;
		TQString group = NMText::connectionGroup(type);
		if (!groups.contains(group)) {
			TDEListViewItem *g = new TDEListViewItem(m_list, group);
			g->setOpen(true);
			g->setSelectable(false);
			TQFont f = m_list->font();
			f.setBold(true);
			groups[group] = g;
		}
		TDEListViewItem *item = new TDEListViewItem(groups[group], (*it).id());
		item->setPixmap(ColName, SmallIcon(NMText::connectionIcon(type)));
		item->setText(ColUsed, lastUsed((*it).timestamp()));
		item->setText(ColPath, (*it).path);
		if ((*it).path == selected || (*it).uuid() == m_pendingSelect || (*it).id() == m_pendingSelect) {
			m_list->setSelected(item, true);
			m_list->setCurrentItem(item);
			m_list->ensureItemVisible(item);
		}
	}
	m_pendingSelect = TQString::null;
	updateStates();
	slotSelectionChanged();
}

void ConnectionList::updateStates()
{
	NMClient *nm = NMClient::self();
	for (TQListViewItemIterator it(m_list); it.current(); ++it) {
		TQString path = it.current()->text(ColPath);
		if (path.isEmpty())
			continue;
		const NMActiveConnection *ac = nm->activeForConnection(path);
		TQString state;
		if (ac) {
			state = NMText::activeState(ac->state);
			if (!ac->devices.isEmpty()) {
				const NMDeviceInfo *d = nm->device(ac->devices.first());
				if (d)
					state = i18n("%1 (%2)").arg(state).arg(d->iface);
			}
		}
		it.current()->setText(ColState, state);
	}
	slotSelectionChanged();
	updateAuthorization();
}

bool ConnectionList::select(const TQString &key)
{
	for (TQListViewItemIterator it(m_list); it.current(); ++it) {
		TQString path = it.current()->text(ColPath);
		const NMConnection *c = NMClient::self()->connection(path);
		if (c && (c->uuid() == key || c->id() == key)) {
			m_list->setSelected(it.current(), true);
			m_list->setCurrentItem(it.current());
			m_list->ensureItemVisible(it.current());
			return true;
		}
	}
	m_pendingSelect = key;
	return false;
}

TQString ConnectionList::selectedPath() const
{
	TQListViewItem *item = m_list->selectedItem();
	return item ? item->text(ColPath) : TQString::null;
}

void ConnectionList::slotSelectionChanged()
{
	TQString path = selectedPath();
	bool sel = !path.isEmpty();
	m_edit->setEnabled(sel);
	m_delete->setEnabled(sel);
	m_duplicate->setEnabled(sel);
	m_connect->setEnabled(sel);
	const NMConnection *c = NMClient::self()->connection(path);
	m_export->setEnabled(c && (c->type() == "vpn" || c->type() == "wireguard"));
	if (sel && NMClient::self()->activeForConnection(path)) {
		m_connect->setText(i18n("Dis&connect"));
		m_connect->setIconSet(SmallIconSet("connect_no"));
	} else {
		m_connect->setText(i18n("&Connect"));
		m_connect->setIconSet(SmallIconSet("connect_established"));
	}
}

void ConnectionList::slotAddMenu(int id)
{
	if (id == 1000) {
		ConnectionSettings c = ConnectionDialog::newConnection("802-11-wireless");
		c.setStr("connection", "id", i18n("Hotspot"));
		c.setBool("connection", "autoconnect", false);
		c.setBytes("802-11-wireless", "ssid", NMDBus::bytes("TDE-Hotspot"));
		c.setStr("802-11-wireless", "mode", "ap");
		c.setStr("802-11-wireless", "band", "bg");
		c.setStr("ipv4", "method", "shared");
		c.setStr("ipv6", "method", "ignore");
		c.setStr("802-11-wireless-security", "key-mgmt", "wpa-psk");
		ConnectionDialog::createConnection("802-11-wireless", this, &c);
		return;
	}
	if (id >= 0 && id < (int)m_addTypes.count())
		addConnection(m_addTypes[id]);
}

void ConnectionList::addConnection(const TQString &type)
{
	TQString path = ConnectionDialog::createConnection(type, this);
	if (!path.isEmpty()) {
		NMClient::self()->refresh();
		const NMConnection *c = NMClient::self()->connection(path);
		if (c)
			select(c->uuid());
	}
}

void ConnectionList::editSelected()
{
	TQString path = selectedPath();
	if (path.isEmpty())
		return;
	ConnectionDialog::editConnection(path, this);
}

void ConnectionList::deleteSelected()
{
	TQString path = selectedPath();
	const NMConnection *c = NMClient::self()->connection(path);
	if (!c)
		return;
	TQString uuid = c->uuid();
	if (KMessageBox::warningContinueCancel(this,
	        i18n("<qt>Do you really want to delete the connection <b>%1</b>?</qt>").arg(c->id()),
	        i18n("Delete Connection"), KStdGuiItem::del()) != KMessageBox::Continue)
		return;
	if (!NMClient::self()->deleteConnection(path))
		KMessageBox::error(this, i18n("<qt>The connection could not be deleted:<br>%1</qt>")
		                         .arg(NMClient::self()->lastError()));
	else
		SecretStore::removeConnection(uuid);
}

void ConnectionList::duplicateSelected()
{
	TQString path = selectedPath();
	if (path.isEmpty())
		return;
	ConnectionSettings conn = ConnectionDialog::withSecrets(path);
	TQString oldUuid = conn.uuid();
	conn.setStr("connection", "id", i18n("%1 (copy)").arg(conn.id()));
	conn.setStr("connection", "uuid", ConnectionSettings::newUuid());
	conn.remove("connection", "timestamp");
	/* copy the user's secrets in the wallet as well */
	TQStringList settings = SecretStore::secretSettings();
	for (TQStringList::const_iterator it = settings.begin(); it != settings.end(); ++it) {
		SecretMap m;
		if (SecretStore::load(oldUuid, *it, &m))
			SecretStore::save(conn.uuid(), *it, m);
	}
	TQString newPath = NMClient::self()->addConnection(conn);
	if (newPath.isEmpty())
		KMessageBox::error(this, NMClient::self()->lastError());
	else
		m_pendingSelect = conn.uuid();
}

void ConnectionList::toggleSelected()
{
	TQString path = selectedPath();
	if (path.isEmpty())
		return;
	NMClient *nm = NMClient::self();
	const NMActiveConnection *ac = nm->activeForConnection(path);
	if (ac)
		nm->deactivate(ac->path);
	else
		nm->activateConnection(path);
}

void ConnectionList::slotOperationFinished(bool ok, const TQString &what, const TQString &error)
{
	if (!ok && isVisible())
		KMessageBox::sorry(this, i18n("<qt><b>%1</b> failed:<br>%2</qt>").arg(what).arg(error));
}

void ConnectionList::slotContextMenu(TQListViewItem *item, const TQPoint &pos, int)
{
	TDEPopupMenu menu(this);
	bool sel = item && !item->text(ColPath).isEmpty();
	if (sel) {
		bool active = NMClient::self()->activeForConnection(item->text(ColPath)) != 0;
		menu.insertItem(SmallIcon(active ? "connect_no" : "connect_established"),
		                active ? i18n("Disconnect") : i18n("Connect"), this, TQ_SLOT(toggleSelected()));
		menu.insertSeparator();
		menu.insertItem(SmallIcon("edit"), i18n("Edit..."), this, TQ_SLOT(editSelected()));
		menu.insertItem(SmallIcon("edit-copy"), i18n("Duplicate"), this, TQ_SLOT(duplicateSelected()));
		menu.insertItem(SmallIcon("edit-delete"), i18n("Delete"), this, TQ_SLOT(deleteSelected()));
		menu.insertSeparator();
	}
	menu.insertItem(SmallIcon("add"), i18n("Add"), m_addMenu);
	menu.exec(pos);
}

static TQString nmcli()
{
	TQString exe = TDEStandardDirs::findExe("nmcli");
	if (exe.isEmpty())
		exe = TDEStandardDirs::findExe("nmcli", "/usr/bin:/bin");
	return exe;
}

static bool runNmcli(const TQStringList &args, TQString *output)
{
	TQString exe = nmcli();
	if (exe.isEmpty()) {
		*output = i18n("The nmcli program (package network-manager) was not found.");
		return false;
	}
	KProcIO proc;
	proc << exe << args;
	if (!proc.start(TDEProcess::Block, true)) {
		*output = i18n("nmcli could not be started.");
		return false;
	}
	TQString line;
	output->truncate(0);
	while (proc.readln(line) >= 0)
		*output += line + "\n";
	return proc.normalExit() && proc.exitStatus() == 0;
}

void ConnectionList::importVpn()
{
	TQString file = KFileDialog::getOpenFileName(TQString::null,
		"*.ovpn *.conf|" + i18n("VPN configuration files (*.ovpn, *.conf)") + "\n*|" + i18n("All files"),
		this, i18n("Import VPN Connection"));
	if (file.isEmpty())
		return;
	/* NetworkManager imports through its VPN plugins; nmcli does the work */
	TQString type = "openvpn";
	TQFile f(file);
	if (f.open(IO_ReadOnly)) {
		TQString text = TQString::fromUtf8(f.readAll());
		if (text.contains("[Interface]") && text.contains("PrivateKey"))
			type = "wireguard";
		else if (text.contains("IPSec gateway", false))
			type = "vpnc";
	}
	TQString output;
	TQStringList args;
	args << "connection" << "import" << "type" << type << "file" << file;
	if (!runNmcli(args, &output)) {
		KMessageBox::detailedSorry(this, i18n("The VPN configuration could not be imported."), output);
		return;
	}
	NMClient::self()->refresh();
}

void ConnectionList::exportSelected()
{
	const NMConnection *c = NMClient::self()->connection(selectedPath());
	if (!c)
		return;
	TQString uuid = c->uuid();
	TQString ext = c->type() == "wireguard" ? ".conf" : ".ovpn";
	TQString file = KFileDialog::getSaveFileName(c->id() + ext, "*" + ext, this,
	                                            i18n("Export VPN Connection"));
	if (file.isEmpty())
		return;
	TQString output;
	TQStringList args;
	args << "connection" << "export" << uuid << file;
	if (!runNmcli(args, &output))
		KMessageBox::detailedSorry(this, i18n("The connection could not be exported."), output);
}

#include "connectionlist.moc"
