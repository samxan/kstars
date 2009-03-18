/*  Device Manager
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    JM Changelog
    2004-16-1:	Start

 */

#include "devicemanager.h"

#include "Options.h"
#include "indimenu.h"
#include "indiproperty.h"
#include "indigroup.h"
#include "indidevice.h"
#include "indistd.h"
#include "indidriver.h"
#include "kstars.h"
#include "kstarsdatetime.h"

#include <indicom.h>

#include <config-kstars.h>

//#include <stdlib.h>
//#include <unistd.h>

#include <QTcpSocket>
#include <QTextEdit>

#include <KProcess>
#include <KLocale>
#include <KDebug>
#include <KMessageBox>
#include <KStatusBar>

const int INDI_MAX_TRIES=3;

/*******************************************************************
** The device manager contain devices running from one indiserver
** This allow KStars to control multiple devices distributed acorss
** multiple servers seemingly in a way that is completely transparent
** to devices and drivers alike.
** The device Manager can be thought of as the 'networking' parent
** of devices, while indimenu is 'GUI' parent of devices
*******************************************************************/
DeviceManager::DeviceManager(INDIMenu *INDIparent, QString inHost, uint inPort, ManagerMode inMode)
  {
    parent		= INDIparent;
    serverProcess	= NULL;
    XMLParser		= NULL;
    host		= inHost;
    port		= inPort;
    mode		= inMode;
}

DeviceManager::~DeviceManager()
{
    if (serverProcess)
	serverProcess->close();
  
    delete (serverProcess);

    if (XMLParser)
        delLilXML(XMLParser);

    XMLParser = NULL;

    while ( ! indi_dev.isEmpty() ) delete indi_dev.takeFirst();
}

void DeviceManager::startServer()
  {
    serverProcess = new KProcess;
  
    if (managed_devices.isEmpty())
      {
	kWarning() << "managed_devices was not set! Cannot start server!";
        return;
      }
  
    *serverProcess << Options::indiServer();
    *serverProcess << "-v" << "-p" << QString::number(port);

    foreach(IDevice *device, managed_devices)
	 *serverProcess << device->driver;

    if (mode == DeviceManager::M_LOCAL)
      {
    	connect(serverProcess, SIGNAL(readyReadStandardError()),  this, SLOT(processStandardError()));
	serverProcess->setOutputChannelMode(KProcess::SeparateChannels);
        serverProcess->setReadChannel(QProcess::StandardError);
      }
  
    serverProcess->start();
  
    serverProcess->waitForStarted();
  
    if (mode == DeviceManager::M_LOCAL)
    	connectToServer();
}
  
void DeviceManager::connectToServer()
{
    connect(&serverSocket, SIGNAL(readyRead()), this, SLOT(dataReceived()));
  
   for (int i=0; i < INDI_MAX_TRIES; i++)
   {
	serverSocket.connectToHost(host, port);
	if (serverSocket.waitForConnected(1000))
	{
		
		connect(&serverSocket, SIGNAL(error( QAbstractSocket::SocketError )), this, SLOT(connectionError()));
		connectionSuccess();
		return;
	}
  
	usleep(100000);
   }
  
   connectionError();
  }
  
void DeviceManager::connectionSuccess()
{
   kDebug() << "Connection success!!";
    
   QTextStream serverFP(&serverSocket);
   
   foreach (IDevice *device, managed_devices)
   		device->state = IDevice::DEV_START;
  
   if (XMLParser)
        delLilXML(XMLParser);
    XMLParser = newLilXML();

    serverFP << QString("<enableBLOB>Also</enableBLOB>\n");
    serverFP << QString("<getProperties version='%1'/>\n").arg(INDIVERSION);
}

void DeviceManager::connectionError()
  {
  QString errMsg = QString("Connection to INDI host at %1 on port %2 encountered an error: %3.").arg(host).arg(port).arg(serverSocket.errorString());
  KMessageBox::error(NULL, errMsg);
  
  emit deviceManagerError(this);
}
  
void DeviceManager::appendManagedDevices(QList<IDevice *> & processed_devices)
{
	managed_devices = processed_devices; 
  
	foreach (IDevice *device, managed_devices)
	{
		device->unique_label 	= parent->getUniqueDeviceLabel(device->tree_label);
		//device->mode		= mode;
		device->deviceManager 	= this;
	}
}
  
void DeviceManager::processStandardError()
{
    if (serverProcess == NULL)
          return;
  
    serverBuffer.append(serverProcess->readAllStandardError());
    emit newServerInput();
}
  
void DeviceManager::dataReceived()
{
    char errmsg[ERRMSG_SIZE];
    int nr=0;
    QTextStream serverFP(&serverSocket);
    QString ibuf, err_cmd;
  
    ibuf = serverFP.readAll();
    nr = ibuf.length();
  
        /* process each char */
       for (int i = 0; i < nr; i++)
       {
           if (!XMLParser)
              return;

           XMLEle *root = readXMLEle (XMLParser, ibuf[i].toAscii(), errmsg);
           if (root)
           {
                if (dispatchCommand(root, err_cmd) < 0)
                {
                    kDebug() << "Dispatch command error: " << err_cmd;
                    prXMLEle (stdout, root, 0);
                }

                delXMLEle (root);
           }
           else if (*errmsg)
                kDebug() << "XML Root Error: " << errmsg;
          }
  }

int DeviceManager::dispatchCommand(XMLEle *root, QString & errmsg)
{

    if  (!strcmp (tagXMLEle(root), "message"))
        return messageCmd(root, errmsg);
    else if  (!strcmp (tagXMLEle(root), "delProperty"))
        return delPropertyCmd(root, errmsg);

    /* Get the device, if not available, create it */
    INDI_D *dp = findDev (root, 1, errmsg);
    if (dp == NULL)
        return -1;

    if (!strcmp (tagXMLEle(root), "defTextVector"))
        return dp->buildTextGUI(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defNumberVector"))
        return dp->buildNumberGUI(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defSwitchVector"))
        return dp->buildSwitchesGUI(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defLightVector"))
        return dp->buildLightsGUI(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "defBLOBVector"))
        return dp->buildBLOBGUI(root, errmsg);
    else if (!strcmp (tagXMLEle(root), "setTextVector") ||
             !strcmp (tagXMLEle(root), "setNumberVector") ||
             !strcmp (tagXMLEle(root), "setSwitchVector") ||
             !strcmp (tagXMLEle(root), "setLightVector") ||
             !strcmp (tagXMLEle(root), "setBLOBVector"))
        return dp->setAnyCmd(root, errmsg);

    return (-1);
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int DeviceManager::delPropertyCmd (XMLEle *root, QString & errmsg)
{
    XMLAtt *ap;
    INDI_D *dp;
    INDI_P *pp;

    /* dig out device and optional property name */
    dp = findDev (root, 0, errmsg);
    if (!dp)
        return (-1);

    checkMsg(root, dp);

    ap = findXMLAtt (root, "name");

    /* Delete property if it exists, otherwise, delete the whole device */
    if (ap)
    {
        pp = dp->findProp(QString(valuXMLAtt(ap)));

        if(pp)
            return dp->removeProperty(pp);
        else
            return (-1);
    }
    // delete the whole device
    else
        return removeDevice(dp->name, errmsg);
}

int DeviceManager::removeDevice( const QString &devName, QString & errmsg )
{
    // remove all devices if devName == NULL
    if (devName == NULL)
    {
        while ( ! indi_dev.isEmpty() ) delete indi_dev.takeFirst();
        return (0);
    }

    for (int i=0; i < indi_dev.size(); i++)
    {
        if (indi_dev[i]->name ==  devName)
        {
            delete indi_dev.takeAt(i);
            return (0);
        }
    }

    errmsg = QString("Device %1 not found").arg(devName);
    return -1;
}

INDI_D * DeviceManager::findDev( const QString &devName, QString & errmsg )
{
    /* search for existing */
    for (int i = 0; i < indi_dev.size(); i++)
    {
        if (indi_dev[i]->name == devName)
            return indi_dev[i];
    }

    errmsg = QString("INDI: no such device %1").arg(devName);

    return NULL;
}

/* add new device to mainrc_w using info in dep.
- * if trouble return NULL with reason in errmsg[]
- */
INDI_D * DeviceManager::addDevice (XMLEle *dep, QString & errmsg)
{
    INDI_D *dp;
    XMLAtt *ap;
    QString device_name, unique_label;

    /* allocate new INDI_D on indi_dev */
    ap = findAtt (dep, "device", errmsg);
    if (!ap)
        return NULL;

    device_name = QString(valuXMLAtt(ap));

        if (mode != M_CLIENT)
    		foreach(IDevice *device, managed_devices)
    		{
		        // Each device manager has a list of managed_devices (IDevice). Each IDevice has the original constant name of the driver (driver_class)
			// Therefore, when a new device is discovered, we match the driver name (which never changes, it's always static from indiserver) against the driver_class
			// of IDevice because IDevice can have several names. It can have the tree_label which is the name it has in the local tree widget. Finally, the name that shows
			// up in the INDI control panel is the unique name of the driver, which is for most cases tree_label, but if that exists already then we get tree_label_1..etc
			if (device->driver_class == device_name && device->state == IDevice::DEV_TERMINATE)
			{
	 			device->state = IDevice::DEV_START;
				unique_label  = device->unique_label;
				break;
			}
		}

	if (unique_label.isEmpty())
		unique_label = parent->getUniqueDeviceLabel(device_name);

    	dp = new INDI_D(parent, this, device_name, unique_label);
	indi_dev.append(dp);
	emit newDevice(dp);
		
	connect(dp->stdDev, SIGNAL(newTelescope()), parent->ksw->indiDriver(), SLOT(newTelescopeDiscovered()));

    	/* ok */
    	return dp;
	
}

INDI_D * DeviceManager::findDev (XMLEle *root, int create, QString & errmsg)
{
    XMLAtt *ap;
    char *dn;

    /* get device name */
    ap = findAtt (root, "device", errmsg);
    if (!ap)
        return (NULL);
    dn = valuXMLAtt(ap);

    /* search for existing */
    for (int i = 0; i < indi_dev.size(); i++)
    {
        if (indi_dev[i]->name == QString(dn))
            return indi_dev[i];
    }

    /* not found, create if ok */
    if (create)
        return (addDevice (root, errmsg));

    errmsg = QString("INDI: <%1> no such device %2").arg(tagXMLEle(root)).arg(dn);
    return NULL;
}

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int DeviceManager::messageCmd (XMLEle *root, QString & errmsg)
{
    checkMsg (root, findDev (root, 0, errmsg));
    return (0);
}

/* display message attribute.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void DeviceManager::checkMsg (XMLEle *root, INDI_D *dp)
{
    XMLAtt *ap;
    ap = findXMLAtt(root, "message");

    if (ap)
        doMsg(root, dp);
}

/* display valu of message and timestamp in dp's scrolled area, if any, else general.
 * prefix our time stamp if not included.
 * N.B. don't put carriage control in msg, we take care of that.
 */
void DeviceManager::doMsg (XMLEle *msg, INDI_D *dp)
{
    QTextEdit *txt_w;
    XMLAtt *message;
    XMLAtt *timestamp;

    if (dp == NULL)
    {
        kDebug() << "Warning: dp is null.";
        return;
    }

    txt_w = dp->msgST_w;

    /* prefix our timestamp if not with msg */
    timestamp = findXMLAtt (msg, "timestamp");

    if (timestamp)
        txt_w->insertPlainText(QString(valuXMLAtt(timestamp)) + QString(" "));
    else
        txt_w->insertPlainText( KStarsDateTime::currentDateTime().toString("yyyy/mm/dd - h:m:s ap "));

    /* finally! the msg */
    message = findXMLAtt(msg, "message");

    if (!message) return;

    txt_w->insertPlainText( QString(valuXMLAtt(message)) + QString("\n"));

    if ( Options::showINDIMessages() )
        parent->ksw->statusBar()->changeItem( QString(valuXMLAtt(message)), 0);

}

void DeviceManager::sendNewText (INDI_P *pp)
{
    INDI_E *lp;

    QTextStream serverFP(&serverSocket);

    serverFP << QString("<newTextVector\n");
    serverFP << QString("  device='%1'\n").arg(qPrintable( pp->pg->dp->name));
    serverFP << QString("  name='%1'\n>").arg(qPrintable( pp->name));

    //for (lp = pp->el.first(); lp != NULL; lp = pp->el.next())
    foreach(lp, pp->el)
    {
        serverFP << QString("  <oneText\n");
        serverFP << QString("    name='%1'>\n").arg(qPrintable( lp->name));
        serverFP << QString("      %1\n").arg(qPrintable( lp->text));
        serverFP << QString("  </oneText>\n");
    }
    serverFP << QString("</newTextVector>\n");
}

void DeviceManager::sendNewNumber (INDI_P *pp)
{
    INDI_E *lp;

    QTextStream serverFP(&serverSocket);

    serverFP << QString("<newNumberVector\n");
    serverFP << QString("  device='%1'\n").arg(qPrintable( pp->pg->dp->name));
    serverFP << QString("  name='%1'\n>").arg(qPrintable( pp->name));

    foreach(lp, pp->el)
    {
        serverFP << QString("  <oneNumber\n");
        serverFP << QString("    name='%1'>\n").arg(qPrintable( lp->name));
        if (lp->text.isEmpty())
        	serverFP << QString("      %1\n").arg(lp->targetValue);
        else
        	serverFP << QString("      %1\n").arg(lp->text);
        serverFP << QString("  </oneNumber>\n");
    }
    serverFP << QString("</newNumberVector>\n");

}

void DeviceManager::sendNewSwitch (INDI_P *pp, INDI_E *lp)
{
    QTextStream serverFP(&serverSocket);

    serverFP << QString("<newSwitchVector\n");
    serverFP << QString("  device='%1'\n").arg(qPrintable( pp->pg->dp->name));
    serverFP << QString("  name='%1'>\n").arg(qPrintable( pp->name));
    serverFP << QString("  <oneSwitch\n");
    serverFP << QString("    name='%1'>\n").arg(qPrintable( lp->name));
    serverFP << QString("      %1\n").arg(lp->state == PS_ON ? "On" : "Off");
    serverFP << QString("  </oneSwitch>\n");

    serverFP <<  QString("</newSwitchVector>\n");

}

void DeviceManager::startBlob( const QString &devName, const QString &propName, const QString &timestamp)
{
    QTextStream serverFP(&serverSocket);

    serverFP <<  QString("<newBLOBVector\n");
    serverFP <<  QString("  device='%1'\n").arg(qPrintable( devName));
    serverFP <<  QString("  name='%1'\n").arg(qPrintable( propName));
    serverFP <<  QString("  timestamp='%1'>\n").arg(qPrintable( timestamp));

}

void DeviceManager::sendOneBlob( const QString &blobName, unsigned int blobSize, const QString &blobFormat, unsigned char * blobBuffer)
{
    QTextStream serverFP(&serverSocket);

    serverFP <<  QString("  <oneBLOB\n");
    serverFP <<  QString("    name='%1'\n").arg(qPrintable( blobName));
    serverFP <<  QString("    size='%1'\n").arg(blobSize);
    serverFP <<  QString("    format='%1'>\n").arg(qPrintable( blobFormat));

    for (unsigned i = 0; i < blobSize; i += 72)
        serverFP <<  QString().sprintf("    %.72s\n", blobBuffer+i);

    serverFP << QString("   </oneBLOB>\n");

}

void DeviceManager::finishBlob()
{
    QTextStream serverFP(&serverSocket);

    serverFP <<  QString("</newBLOBVector>\n");
}

#include "devicemanager.moc"