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

#include "pages.h"
#include "connectiondialog.h"
#include "nmclient.h"
#include "nmtext.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqgroupbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>
#include <tqregexp.h>
#include <tqspinbox.h>
#include <tqtextedit.h>

#include <kiconloader.h>
#include <klineedit.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

static TQString validInterfaceName(const TQString &name)
{
	if (name.isEmpty())
		return i18n("The interface name is missing.");
	if (name.length() > 15 || name.contains('/') || name.contains(TQRegExp("\\s")))
		return i18n("\"%1\" is not a valid interface name (at most 15 characters, no spaces).").arg(name);
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* interface name only                                                    */
/* ---------------------------------------------------------------------- */

InterfacePage::InterfacePage(TQWidget *parent)
	: SettingPage(parent, "InterfacePage")
{
	TQGridLayout *g = new TQGridLayout(this, 3, 2, 8, 6);
	m_iface = new KLineEdit(this);
	addRow(g, 0, i18n("&Interface name:"), m_iface);
	m_mtu = new TQSpinBox(0, 65535, 1, this);
	m_mtu->setSpecialValueText(i18n("Automatic"));
	addRow(g, 1, i18n("MT&U:"), m_mtu);
	g->setRowStretch(2, 1);
	g->setColStretch(1, 1);
}

void InterfacePage::load(const ConnectionSettings &conn)
{
	m_type = conn.type();
	m_iface->setText(conn.interfaceName());
	m_mtu->setValue(conn.uint("ethernet", "mtu"));
}

void InterfacePage::save(ConnectionSettings &conn)
{
	conn.setStr("connection", "interface-name", m_iface->text().stripWhiteSpace());
}

TQString InterfacePage::validate() const
{
	if (m_type == "dummy" || m_type == "tun" || m_type == "loopback")
		return validInterfaceName(m_iface->text().stripWhiteSpace());
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* ports of a bond, bridge or team                                        */
/* ---------------------------------------------------------------------- */

PortsWidget::PortsWidget(const TQString &portType, TQWidget *parent)
	: TQWidget(parent, "PortsWidget"), m_portType(portType)
{
	TQGridLayout *g = new TQGridLayout(this, 4, 2, 0, 6);
	m_list = new TDEListView(this);
	m_list->addColumn(i18n("Port"));
	m_list->addColumn(i18n("Device"));
	m_list->setAllColumnsShowFocus(true);
	m_list->setResizeMode(TQListView::LastColumn);
	m_list->setMinimumHeight(100);
	g->addMultiCellWidget(m_list, 0, 3, 0, 0);
	TQPushButton *add = new TQPushButton(i18n("&Add..."), this);
	m_edit = new TQPushButton(i18n("&Edit..."), this);
	m_delete = new TQPushButton(i18n("De&lete"), this);
	g->addWidget(add, 0, 1);
	g->addWidget(m_edit, 1, 1);
	g->addWidget(m_delete, 2, 1);
	g->setRowStretch(3, 1);
	connect(add, TQ_SIGNAL(clicked()), TQ_SLOT(slotAdd()));
	connect(m_edit, TQ_SIGNAL(clicked()), TQ_SLOT(slotEdit()));
	connect(m_delete, TQ_SIGNAL(clicked()), TQ_SLOT(slotDelete()));
	connect(m_list, TQ_SIGNAL(executed(TQListViewItem*)), TQ_SLOT(slotEdit()));
	connect(NMClient::self(), TQ_SIGNAL(connectionsChanged()), TQ_SLOT(reload()));
}

void PortsWidget::setController(const TQString &uuid, const TQString &iface)
{
	m_uuid = uuid;
	m_iface = iface;
	reload();
}

void PortsWidget::reload()
{
	m_list->clear();
	const NMConnectionList &all = NMClient::self()->connections();
	for (NMConnectionList::const_iterator it = all.begin(); it != all.end(); ++it) {
		const ConnectionSettings &s = (*it).settings;
		TQString master = s.str("connection", "controller", s.str("connection", "master"));
		if (master.isEmpty() || (master != m_uuid && master != m_iface))
			continue;
		TDEListViewItem *item = new TDEListViewItem(m_list, (*it).id(), s.interfaceName());
		item->setPixmap(0, SmallIcon(NMText::connectionIcon((*it).type())));
		item->setText(2, (*it).path);
	}
	m_edit->setEnabled(m_list->childCount() > 0);
	m_delete->setEnabled(m_list->childCount() > 0);
}

void PortsWidget::slotAdd()
{
	ConnectionSettings c = ConnectionDialog::newConnection("802-3-ethernet");
	c.setStr("connection", "id", i18n("%1 port %2").arg(m_iface.isEmpty() ? m_portType : m_iface)
	                              .arg(m_list->childCount() + 1));
	c.setStr("connection", "master", m_uuid);
	c.setStr("connection", "slave-type", m_portType);
	c.removeSetting("ipv4");
	c.removeSetting("ipv6");
	ConnectionDialog::createConnection("802-3-ethernet", this, &c);
}

void PortsWidget::slotEdit()
{
	TQListViewItem *item = m_list->currentItem();
	if (item)
		ConnectionDialog::editConnection(item->text(2), this);
}

void PortsWidget::slotDelete()
{
	TQListViewItem *item = m_list->currentItem();
	if (!item)
		return;
	if (KMessageBox::warningContinueCancel(this, i18n("Delete the port connection \"%1\"?").arg(item->text(0)),
	                                       i18n("Delete Port"), KStdGuiItem::del()) != KMessageBox::Continue)
		return;
	if (!NMClient::self()->deleteConnection(item->text(2)))
		KMessageBox::error(this, NMClient::self()->lastError());
}

/* ---------------------------------------------------------------------- */
/* bond                                                                   */
/* ---------------------------------------------------------------------- */

static TQStringList bondModes()
{
	TQStringList m;
	m << "balance-rr" << "active-backup" << "balance-xor" << "broadcast" << "802.3ad"
	  << "balance-tlb" << "balance-alb";
	return m;
}

BondPage::BondPage(TQWidget *parent)
	: SettingPage(parent, "BondPage")
{
	TQGridLayout *g = new TQGridLayout(this, 10, 2, 8, 6);
	int row = 0;
	m_iface = new KLineEdit(this);
	addRow(g, row++, i18n("&Interface name:"), m_iface);

	TQGroupBox *box = new TQGroupBox(1, TQt::Horizontal, i18n("Ports"), this);
	m_ports = new PortsWidget("bond", box);
	g->addMultiCellWidget(box, row, row, 0, 1);
	row++;

	m_mode = new TQComboBox(false, this);
	m_mode->insertItem(i18n("Round-robin"));
	m_mode->insertItem(i18n("Active backup"));
	m_mode->insertItem(i18n("XOR"));
	m_mode->insertItem(i18n("Broadcast"));
	m_mode->insertItem(i18n("802.3ad (LACP)"));
	m_mode->insertItem(i18n("Adaptive transmit load balancing"));
	m_mode->insertItem(i18n("Adaptive load balancing"));
	addRow(g, row++, i18n("&Mode:"), m_mode);
	m_primary = new KLineEdit(this);
	addRow(g, row++, i18n("&Primary:"), m_primary);

	m_monitoring = new TQComboBox(false, this);
	m_monitoring->insertItem(i18n("MII (recommended)"));
	m_monitoring->insertItem(i18n("ARP"));
	addRow(g, row++, i18n("Link mo&nitoring:"), m_monitoring);
	m_frequency = new TQSpinBox(0, 999999, 10, this);
	m_frequency->setSuffix(i18n(" ms"));
	addRow(g, row++, i18n("Monitoring &frequency:"), m_frequency);
	m_updelay = new TQSpinBox(0, 999999, 10, this);
	m_updelay->setSuffix(i18n(" ms"));
	addRow(g, row++, i18n("Link &up delay:"), m_updelay);
	m_downdelay = new TQSpinBox(0, 999999, 10, this);
	m_downdelay->setSuffix(i18n(" ms"));
	addRow(g, row++, i18n("Link &down delay:"), m_downdelay);
	m_arpTargets = new KLineEdit(this);
	addRow(g, row++, i18n("ARP &targets:"), m_arpTargets);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

void BondPage::load(const ConnectionSettings &conn)
{
	m_iface->setText(conn.interfaceName());
	m_options = conn.strMap("bond", "options");
	setComboData(m_mode, bondModes(), m_options.contains("mode") ? m_options["mode"] : TQString("balance-rr"));
	m_primary->setText(m_options["primary"]);
	bool arp = m_options.contains("arp_interval") && m_options["arp_interval"].toInt() > 0;
	m_monitoring->setCurrentItem(arp ? 1 : 0);
	m_frequency->setValue(arp ? m_options["arp_interval"].toInt()
	                          : (m_options.contains("miimon") ? m_options["miimon"].toInt() : 100));
	m_updelay->setValue(m_options["updelay"].toInt());
	m_downdelay->setValue(m_options["downdelay"].toInt());
	m_arpTargets->setText(m_options["arp_ip_target"]);
	m_ports->setController(conn.uuid(), conn.interfaceName());
}

void BondPage::save(ConnectionSettings &conn)
{
	conn.setStr("connection", "interface-name", m_iface->text().stripWhiteSpace());
	TQMap<TQString, TQString> o = m_options;
	o["mode"] = comboData(m_mode, bondModes());
	o.remove("miimon");
	o.remove("arp_interval");
	o.remove("arp_ip_target");
	o.remove("updelay");
	o.remove("downdelay");
	if (m_monitoring->currentItem() == 0) {
		o["miimon"] = TQString::number(m_frequency->value());
		if (m_updelay->value())
			o["updelay"] = TQString::number(m_updelay->value());
		if (m_downdelay->value())
			o["downdelay"] = TQString::number(m_downdelay->value());
	} else {
		o["arp_interval"] = TQString::number(m_frequency->value());
		o["arp_ip_target"] = m_arpTargets->text().stripWhiteSpace();
	}
	TQString primary = m_primary->text().stripWhiteSpace();
	if (primary.isEmpty())
		o.remove("primary");
	else
		o["primary"] = primary;
	conn.setStrMap("bond", "options", o);
}

TQString BondPage::validate() const
{
	return validInterfaceName(m_iface->text().stripWhiteSpace());
}

/* ---------------------------------------------------------------------- */
/* bridge                                                                 */
/* ---------------------------------------------------------------------- */

BridgePage::BridgePage(TQWidget *parent)
	: SettingPage(parent, "BridgePage")
{
	TQGridLayout *g = new TQGridLayout(this, 11, 2, 8, 6);
	int row = 0;
	m_iface = new KLineEdit(this);
	addRow(g, row++, i18n("&Interface name:"), m_iface);

	TQGroupBox *box = new TQGroupBox(1, TQt::Horizontal, i18n("Ports"), this);
	m_ports = new PortsWidget("bridge", box);
	g->addMultiCellWidget(box, row, row, 0, 1);
	row++;

	m_ageing = new TQSpinBox(0, 1000000, 1, this);
	m_ageing->setSuffix(i18n(" s"));
	addRow(g, row++, i18n("&Aging time:"), m_ageing);
	m_multicastSnooping = new TQCheckBox(i18n("Enable IGMP &snooping"), this);
	g->addWidget(m_multicastSnooping, row++, 1);
	m_stp = new TQCheckBox(i18n("Enable &STP (Spanning Tree Protocol)"), this);
	g->addWidget(m_stp, row++, 1);
	m_priority = new TQSpinBox(0, 65535, 1, this);
	addRow(g, row++, i18n("STP &priority:"), m_priority);
	m_forwardDelay = new TQSpinBox(2, 30, 1, this);
	m_forwardDelay->setSuffix(i18n(" s"));
	addRow(g, row++, i18n("&Forward delay:"), m_forwardDelay);
	m_helloTime = new TQSpinBox(1, 10, 1, this);
	m_helloTime->setSuffix(i18n(" s"));
	addRow(g, row++, i18n("&Hello time:"), m_helloTime);
	m_maxAge = new TQSpinBox(6, 40, 1, this);
	m_maxAge->setSuffix(i18n(" s"));
	addRow(g, row++, i18n("&Max age:"), m_maxAge);
	m_vlanFiltering = new TQCheckBox(i18n("Enable &VLAN filtering"), this);
	g->addWidget(m_vlanFiltering, row++, 1);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
	connect(m_stp, TQ_SIGNAL(toggled(bool)), m_priority, TQ_SLOT(setEnabled(bool)));
	connect(m_stp, TQ_SIGNAL(toggled(bool)), m_forwardDelay, TQ_SLOT(setEnabled(bool)));
	connect(m_stp, TQ_SIGNAL(toggled(bool)), m_helloTime, TQ_SLOT(setEnabled(bool)));
	connect(m_stp, TQ_SIGNAL(toggled(bool)), m_maxAge, TQ_SLOT(setEnabled(bool)));
}

void BridgePage::load(const ConnectionSettings &conn)
{
	m_iface->setText(conn.interfaceName());
	m_ageing->setValue(conn.uint("bridge", "ageing-time", 300));
	m_multicastSnooping->setChecked(conn.boolean("bridge", "multicast-snooping", true));
	m_stp->setChecked(conn.boolean("bridge", "stp", true));
	m_priority->setValue(conn.uint("bridge", "priority", 32768));
	m_forwardDelay->setValue(conn.uint("bridge", "forward-delay", 15));
	m_helloTime->setValue(conn.uint("bridge", "hello-time", 2));
	m_maxAge->setValue(conn.uint("bridge", "max-age", 20));
	m_vlanFiltering->setChecked(conn.boolean("bridge", "vlan-filtering"));
	m_priority->setEnabled(m_stp->isChecked());
	m_forwardDelay->setEnabled(m_stp->isChecked());
	m_helloTime->setEnabled(m_stp->isChecked());
	m_maxAge->setEnabled(m_stp->isChecked());
	m_ports->setController(conn.uuid(), conn.interfaceName());
}

void BridgePage::save(ConnectionSettings &conn)
{
	conn.setStr("connection", "interface-name", m_iface->text().stripWhiteSpace());
	conn.setUInt("bridge", "ageing-time", m_ageing->value());
	conn.setBool("bridge", "multicast-snooping", m_multicastSnooping->isChecked());
	conn.setBool("bridge", "stp", m_stp->isChecked());
	conn.setUInt("bridge", "priority", m_priority->value());
	conn.setUInt("bridge", "forward-delay", m_forwardDelay->value());
	conn.setUInt("bridge", "hello-time", m_helloTime->value());
	conn.setUInt("bridge", "max-age", m_maxAge->value());
	conn.setBool("bridge", "vlan-filtering", m_vlanFiltering->isChecked());
}

TQString BridgePage::validate() const
{
	return validInterfaceName(m_iface->text().stripWhiteSpace());
}

/* ---------------------------------------------------------------------- */
/* team                                                                   */
/* ---------------------------------------------------------------------- */

TeamPage::TeamPage(TQWidget *parent)
	: SettingPage(parent, "TeamPage")
{
	TQGridLayout *g = new TQGridLayout(this, 5, 2, 8, 6);
	m_iface = new KLineEdit(this);
	addRow(g, 0, i18n("&Interface name:"), m_iface);
	TQGroupBox *box = new TQGroupBox(1, TQt::Horizontal, i18n("Ports"), this);
	m_ports = new PortsWidget("team", box);
	g->addMultiCellWidget(box, 1, 1, 0, 1);
	m_runner = new TQComboBox(false, this);
	m_runner->insertItem("roundrobin");
	m_runner->insertItem("broadcast");
	m_runner->insertItem("activebackup");
	m_runner->insertItem("loadbalance");
	m_runner->insertItem("lacp");
	m_runner->insertItem("random");
	addRow(g, 2, i18n("&Runner:"), m_runner);
	m_config = new TQTextEdit(this);
	m_config->setTextFormat(TQt::PlainText);
	TQLabel *l = addRow(g, 3, i18n("JSON &configuration:"), m_config);
	l->setAlignment(TQt::AlignTop);
	TQLabel *note = new TQLabel(i18n("<qt>The JSON configuration overrides the runner; "
	                                 "see <i>man teamd.conf</i>.</qt>"), this);
	g->addWidget(note, 4, 1);
	g->setColStretch(1, 1);
	g->setRowStretch(3, 1);
}

void TeamPage::load(const ConnectionSettings &conn)
{
	m_iface->setText(conn.interfaceName());
	TQString runner = conn.str("team", "runner", "roundrobin");
	for (int i = 0; i < m_runner->count(); ++i)
		if (m_runner->text(i) == runner)
			m_runner->setCurrentItem(i);
	m_config->setText(conn.str("team", "config"));
	m_ports->setController(conn.uuid(), conn.interfaceName());
}

void TeamPage::save(ConnectionSettings &conn)
{
	conn.ensure("team");
	conn.setStr("connection", "interface-name", m_iface->text().stripWhiteSpace());
	TQString json = m_config->text().stripWhiteSpace();
	conn.setStr("team", "config", json);
	if (json.isEmpty())
		conn.setStr("team", "runner", m_runner->currentText());
}

TQString TeamPage::validate() const
{
	return validInterfaceName(m_iface->text().stripWhiteSpace());
}

/* ---------------------------------------------------------------------- */
/* VLAN                                                                   */
/* ---------------------------------------------------------------------- */

VlanPage::VlanPage(TQWidget *parent)
	: SettingPage(parent, "VlanPage"), m_nameEdited(false)
{
	TQGridLayout *g = new TQGridLayout(this, 8, 2, 8, 6);
	int row = 0;
	m_parent = new TQComboBox(true, this);
	fillInterfaces(m_parent, 0, false);
	addRow(g, row++, i18n("&Parent interface:"), m_parent);
	m_id = new TQSpinBox(0, 4094, 1, this);
	addRow(g, row++, i18n("VLAN &ID:"), m_id);
	m_iface = new KLineEdit(this);
	addRow(g, row++, i18n("VLAN &interface name:"), m_iface);
	m_reorder = new TQCheckBox(i18n("&Reorder headers"), this);
	g->addWidget(m_reorder, row++, 1);
	m_gvrp = new TQCheckBox(i18n("Use &GVRP"), this);
	g->addWidget(m_gvrp, row++, 1);
	m_mvrp = new TQCheckBox(i18n("Use &MVRP"), this);
	g->addWidget(m_mvrp, row++, 1);
	m_looseBinding = new TQCheckBox(i18n("&Loose binding"), this);
	g->addWidget(m_looseBinding, row++, 1);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
	connect(m_parent, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotUpdateName()));
	connect(m_parent, TQ_SIGNAL(activated(int)), TQ_SLOT(slotUpdateName()));
	connect(m_id, TQ_SIGNAL(valueChanged(int)), TQ_SLOT(slotUpdateName()));
}

void VlanPage::slotUpdateName()
{
	if (m_nameEdited)
		return;
	TQString parent = m_parent->currentText().stripWhiteSpace();
	if (!parent.isEmpty())
		m_iface->setText(TQString("%1.%2").arg(parent).arg(m_id->value()).left(15));
}

void VlanPage::load(const ConnectionSettings &conn)
{
	m_parent->setEditText(conn.str("vlan", "parent"));
	m_id->setValue(conn.uint("vlan", "id"));
	m_iface->setText(conn.interfaceName());
	/* only suggest a name for new VLANs; NetworkManager may name it */
	m_nameEdited = !conn.interfaceName().isEmpty() || !conn.str("vlan", "parent").isEmpty();
	uint flags = conn.uint("vlan", "flags", 1);
	m_reorder->setChecked(flags & 0x1);
	m_gvrp->setChecked(flags & 0x2);
	m_looseBinding->setChecked(flags & 0x4);
	m_mvrp->setChecked(flags & 0x8);
	if (!m_nameEdited)
		slotUpdateName();
}

void VlanPage::save(ConnectionSettings &conn)
{
	conn.setStr("vlan", "parent", m_parent->currentText().stripWhiteSpace());
	conn.setUInt("vlan", "id", m_id->value());
	conn.setStr("connection", "interface-name", m_iface->text().stripWhiteSpace());
	uint flags = (m_reorder->isChecked() ? 0x1 : 0) | (m_gvrp->isChecked() ? 0x2 : 0) |
	             (m_looseBinding->isChecked() ? 0x4 : 0) | (m_mvrp->isChecked() ? 0x8 : 0);
	conn.setUInt("vlan", "flags", flags);
}

TQString VlanPage::validate() const
{
	if (m_parent->currentText().stripWhiteSpace().isEmpty())
		return i18n("Choose the parent interface of the VLAN.");
	TQString name = m_iface->text().stripWhiteSpace();
	return name.isEmpty() ? TQString::null : validInterfaceName(name);
}
