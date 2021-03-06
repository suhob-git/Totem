#include "designnetview.h"
#include <QAction>
#include <QApplication>
#include <QDropEvent>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QMultiHash>
#include <QtOpenGL\QGLWidget>
#include "../../coreplugin/actionmanager/actionmanager.h"
#include "../../coreplugin/actionmanager/command.h"
#include "../../coreplugin/icore.h"
#include "../../coreplugin/messagemanager.h"
#include "designnetbase/processor.h"
#include "designnetbase/processorfactory.h"
#include "graphicsitem/blocktextitem.h"
#include "graphicsitem/portarrowlink.h"
#include "graphicsitem/portitem.h"
#include "graphicsitem/processorarrowlink.h"
#include "graphicsitem/processorgraphicsblock.h"
#include "Utils/totemassert.h"
#include "Utils/XML/xmldeserializer.h"
#include "Utils/XML/xmlserializer.h"
#include "designnetconstants.h"
#include "designnetspace.h"


namespace DesignNet{


class GraphicsConnection : public Utils::XmlSerializable
{
public:

	DECLARE_SERIALIZABLE_NOTYPE(GraphicsConnection)

	GraphicsConnection() : m_srcProcessor(-1), m_targetProcessor(-1), m_srcPort(-1), m_targetPort(-1), m_pt1(0, 0), m_pt2(0, 0) {}
	GraphicsConnection(const GraphicsConnection &c)
	{
		m_srcProcessor	= c.m_srcProcessor;
		m_srcPort		= c.m_srcPort;
		m_targetProcessor = c.m_targetProcessor;
		m_targetPort	= c.m_targetPort;
		m_pt1			= c.m_pt1;
		m_pt2			= c.m_pt2;
	}

	virtual void serialize(Utils::XmlSerializer& s) const
	{
		s.serialize("SrcProcessor", m_srcProcessor);
		s.serialize("TargetProcessor", m_targetProcessor);
		s.serialize("SrcPort", m_srcPort);
		s.serialize("TargetPort", m_targetPort);
		s.serialize("pt1.x", m_pt1.x());
		s.serialize("pt1.y", m_pt1.y());
		s.serialize("pt2.x", m_pt2.x());
		s.serialize("pt2.y", m_pt2.y());
	}

	virtual void deserialize(Utils::XmlDeserializer& s)
	{
		s.deserialize("SrcProcessor", m_srcProcessor);
		s.deserialize("TargetProcessor", m_targetProcessor);
		s.deserialize("SrcPort", m_srcPort);
		s.deserialize("TargetPort", m_targetPort);
		int x, y;
		s.deserialize("pt1.x", x);
		s.deserialize("pt1.y", y);
		m_pt1.setX(x);
		m_pt1.setY(y);
		s.deserialize("pt2.x", x);
		s.deserialize("pt2.y", y);
		m_pt2.setX(x);
		m_pt2.setY(y);
	}

	int			m_srcProcessor;
	int			m_srcPort;
	int			m_targetProcessor;
	int			m_targetPort;
	QPointF		m_pt1;
	QPointF		m_pt2;
};



class DesignNetViewPrivate
{
public:
	DesignNetViewPrivate();
	~DesignNetViewPrivate();
	DesignNetSpace*								m_designnetSpace;
	QMap<Processor*, ProcessorGraphicsBlock*>	m_processorMaps;
	QList<ProcessorArrowLink*>					m_links;
	ProcessorGraphicsBlock*						m_tempProcessor;
	ProcessorGraphicsBlock*						m_srcProcessor;
	QGraphicsLineItem*							m_lineItem;
};

DesignNetViewPrivate::DesignNetViewPrivate()
{
	m_designnetSpace = 0;
	m_tempProcessor = 0;
	m_lineItem = 0;
	m_srcProcessor = 0;
}

DesignNetViewPrivate::~DesignNetViewPrivate()
{
}


DesignNetView::DesignNetView(DesignNetSpace *space, QWidget *parent)
	: QGraphicsView(parent),
	d(new DesignNetViewPrivate)
{
	d->m_designnetSpace = space;
	d->m_lineItem = new QGraphicsLineItem(0);
	QPen pen(QBrush(QColor(255, 20, 0, 180)), 3, Qt::DotLine);
	d->m_lineItem->setPen(pen);
	m_bLinking = false;
	m_bPressed = false;
	setObjectName(QLatin1String("DesignView"));
 	setRenderHint(QPainter::Antialiasing);
	QGraphicsScene *scene = new QGraphicsScene(-1000, -1000, 2000, 2000, this);
	setScene(scene);
	viewport()->setMouseTracking(true);
 	setFocusPolicy(Qt::StrongFocus);
 	setDragMode(QGraphicsView::RubberBandDrag);
	scene->setBackgroundBrush(Qt::black);
	d->m_lineItem->setVisible(false);
	scene->addItem(d->m_lineItem);
	setAcceptDrops(true);
	setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
	setCacheMode(QGraphicsView::CacheBackground);
	if (space)
	{
		QObject::connect(space, SIGNAL(logout(QString)), this, SLOT(OnShowMessage(QString)));
		QObject::connect(space, SIGNAL(processorAdded(Processor*)), this, SLOT(onProcessorAdded(Processor*)));
		QObject::connect(space, SIGNAL(processorRemoved(Processor*)), this, SLOT(onProcessorRemoved(Processor*)));
		QObject::connect(space, SIGNAL(connectionAdded(Processor*, Processor*)), this, SLOT(onConnectionAdded(Processor*, Processor*)));
		QObject::connect(space, SIGNAL(connectionRemoved(Processor*, Processor*)), this, SLOT(onConnectionAdded(Processor*, Processor*)));
	}
}

DesignNetView::~DesignNetView()
{
	delete d;
}

void DesignNetView::dragLeaveEvent(QDragLeaveEvent *event)
{
	QGraphicsView::dragLeaveEvent(event);
}

void DesignNetView::dragMoveEvent(QDragMoveEvent *event)
{
	if(event->mimeData()->hasFormat(Constants::MIME_TYPE_TOOLITEM))
	{
		event->setDropAction(Qt::CopyAction);
		event->accept();
	}
	else
	{
		event->ignore();
	}
}
void DesignNetView::dropEvent( QDropEvent * event )
{
	if(!event->mimeData()->hasFormat(Constants::MIME_TYPE_TOOLITEM))
		return;
	QPointF point = mapToScene(event->pos());

	QString processorName = QString::fromLatin1(event->mimeData()->data(Constants::MIME_TYPE_TOOLITEM).data());
	Processor *processor = ProcessorFactory::instance()->create(0, processorName);
	d->m_designnetSpace->addProcessor(processor, true);
	
	scene()->clearSelection();
	ProcessorGraphicsBlock* pBlock = d->m_processorMaps[processor];
	if (pBlock)
	{
		pBlock->setSelected(true);
		pBlock->setPos(point);
	}
	QGraphicsView::dropEvent(event);
}

void DesignNetView::dragEnterEvent( QDragEnterEvent * event )
{
	if(event->mimeData()->hasFormat(Constants::MIME_TYPE_TOOLITEM))
	{
		event->setDropAction(Qt::CopyAction);
		event->accept();
	}
	else
	{
		event->ignore();
	}
}

void DesignNetView::setDesignNetSpace( DesignNetSpace *space )
{
	if (d->m_designnetSpace)
	{
		d->m_designnetSpace->removeProcessor(0);
		d->m_designnetSpace->QObject::disconnect();
	}
	d->m_designnetSpace = space;
	if (space)
	{
		QObject::connect(space, SIGNAL(logout(QString)), this, SLOT(OnShowMessage(QString)));
		QObject::connect(space, SIGNAL(processorAdded(Processor*)), this, SLOT(onProcessorAdded(Processor*)));
		QObject::connect(space, SIGNAL(processorRemoved(Processor*)), this, SLOT(onProcessorRemoved(Processor*)));
		QObject::connect(space, SIGNAL(connectionAdded(Processor*, Processor*)), this, SLOT(onConnectionAdded(Processor*, Processor*)));
		QObject::connect(space, SIGNAL(connectionRemoved(Processor*, Processor*)), this, SLOT(onConnectionRemoved(Processor*, Processor*)));
		QObject::connect(space, SIGNAL(connectionRemoved(Port*, Port*)), this, SLOT(onConnectionRemoved(Port*, Port*)));
	}
}

DesignNetSpace* DesignNetView::getSpace() const
{
	return d->m_designnetSpace;
}

void DesignNetView::processorClosed()
{
	ProcessorGraphicsBlock *block = qobject_cast<ProcessorGraphicsBlock*>(sender());
	removeProcessor(block);
}

void DesignNetView::processorPortVisibleChanged(bool bVisible, int iType)
{
	ProcessorGraphicsBlock *block = qobject_cast<ProcessorGraphicsBlock*>(sender());
	QList<ProcessorArrowLink*>::iterator itr = d->m_links.begin();
	while (itr != d->m_links.end())
	{
		if ((*itr)->getSrc() == block && iType > 0)
		{
			(*itr)->getTarget()->setPortVisible(bVisible, -iType);
			(*itr)->setVisible(!bVisible);
		}
		else if ((*itr)->getTarget() == block && iType < 0)
		{
			(*itr)->getSrc()->setPortVisible(bVisible, -iType);
			(*itr)->setVisible(!bVisible);
		}
		itr++;
	}
}

void DesignNetView::keyReleaseEvent( QKeyEvent *keyEvent )
{
	if(keyEvent->key() == Qt::Key_Delete)
	{
		QList<QGraphicsItem*> items = scene()->selectedItems();
		removeItems(items);
	}
	else if (keyEvent->key() == Qt::Key_Escape)
	{
		setEditState(EditState_Move);
	}
	keyEvent->accept();
}

void DesignNetView::removeItems(QList<QGraphicsItem*> items)
{
	scene()->blockSignals(true);
    /*!
      \todo 在某些情况下是不允许删除的
    */

	foreach(QGraphicsItem *item, items)// 删除PortConnect
	{
		if(item->type() == PortArrowLink::Type)
		{
			PortArrowLink *portLinkItem = qgraphicsitem_cast<PortArrowLink*>(item);
			if(portLinkItem)
				portLinkItem->getSrc()->port()->disconnect(portLinkItem->getTarget()->port());

			items.removeAll(item);
		}
	}

	foreach(QGraphicsItem *item, items)// 删除ProcessorConnect
	{
		if(item->type() == ProcessorArrowLink::Type)
		{
			ProcessorArrowLink *processorLinkItem = qgraphicsitem_cast<ProcessorArrowLink*>(item);
			if(processorLinkItem)
				processorLinkItem->getSrc()->processor()->disconnect(processorLinkItem->getTarget()->processor());

			items.removeAll(item);
		}
	}

	foreach(QGraphicsItem *item, items)// 最后删除Processor
    {
        if(item->type() == ProcessorGraphicsBlock::Type)
		{
            ProcessorGraphicsBlock *portItem = qgraphicsitem_cast<ProcessorGraphicsBlock*>(item);
            if(portItem)
				removeProcessor(portItem);
		
			items.removeAll(item);
        }
    }
	scene()->blockSignals(false);
}

void DesignNetView::mousePressEvent( QMouseEvent * event )
{
	qDebug() << "mouse press";
	QPointF scenePos = mapToScene(event->pos());
	QGraphicsItem *pItem = scene()->itemAt(scenePos, QTransform());
	if (!pItem || pItem->type() != ProcessorGraphicsBlock::Type)
	{
		event->accept();
	}
	else if(m_eEditState == EditState_Link)
	{
		m_bPressed = true;
		d->m_srcProcessor = (ProcessorGraphicsBlock*)pItem;
		d->m_srcProcessor->setHover(true);
		d->m_lineItem->setVisible(true);
		d->m_lineItem->setLine(scenePos.x(), scenePos.y(), scenePos.x(), scenePos.y());
	}

	QGraphicsView::mousePressEvent(event);
}

void DesignNetView::mouseMoveEvent( QMouseEvent * event )
{
	if (m_eEditState == EditState_Link && m_bPressed)
	{
		QPointF scenePos = mapToScene(event->pos());
		QPointF p = d->m_srcProcessor->scenePos();
		QPointF posSrc = d->m_srcProcessor->getCrossPoint(QLineF(p, scenePos));
		d->m_lineItem->setLine(p.x(), p.y(), scenePos.x(), scenePos.y());

		QGraphicsItem *pItem = itemAt(event->pos());
		if (!pItem || pItem->type() != ProcessorGraphicsBlock::Type)
		{
			if (d->m_tempProcessor && d->m_tempProcessor != d->m_srcProcessor)
				d->m_tempProcessor->setHover(false);
			d->m_tempProcessor = 0;
			return;
		}
		
		d->m_tempProcessor = (ProcessorGraphicsBlock*)pItem;
		d->m_tempProcessor->setEmphasized(true);
	}
	QGraphicsView::mouseMoveEvent(event);
}

void DesignNetView::mouseReleaseEvent( QMouseEvent * event )
{
	QGraphicsView::mouseReleaseEvent(event);
	if (m_eEditState == EditState_Link && m_bPressed)
	{
		if (d->m_tempProcessor && d->m_tempProcessor != d->m_srcProcessor)
		{
			d->m_tempProcessor->setHover(true);
			d->m_srcProcessor->processor()->connectTo(d->m_tempProcessor->processor());
		}
	}
	else if (m_eEditState == EditState_Move)
	{
		QGraphicsItem *pItem = itemAt(event->pos());
		if (pItem && pItem->type() == ProcessorGraphicsBlock::Type)
			emit showAvailiableData(((ProcessorGraphicsBlock*)pItem)->processor());
	}
	if (m_bPressed)
	{
		m_bPressed = false;
		d->m_tempProcessor = 0;
		d->m_lineItem->setVisible(false);
	}
}

void DesignNetView::addProcessor(ProcessorGraphicsBlock *processor)
{
	d->m_processorMaps[processor->processor()] = processor;
	QObject::connect(processor, SIGNAL(closed()), this, SLOT(processorClosed()));
	QObject::connect(processor, SIGNAL(portVisibleChanged(bool, int)), this, SLOT(processorPortVisibleChanged(bool, int)));
}

void DesignNetView::removeProcessor(ProcessorGraphicsBlock *item)
{
	if (item)
	{
		Processor *processor = item->processor();
		if (!processor->isRunning())
			d->m_designnetSpace->removeProcessor(processor, true);
	}
}

void DesignNetView::reloadSpace()
{

}

ProcessorGraphicsBlock * DesignNetView::getGraphicsProcessor( const int &id )
{
	foreach(Processor *p, d->m_processorMaps.keys())
	{
		if(p->id() == id)
			return d->m_processorMaps[p];
	}
	return 0;
}

PortItem* DesignNetView::getPortItem(Port* port)
{
	ProcessorGraphicsBlock* processor = getGraphicsProcessor(port->processor()->id());
	if (!processor)
		return 0;

	return processor->cvtPortItem(port);
}

void DesignNetView::serialize(Utils::XmlSerializer &s)
{
	QMap<Processor*, ProcessorGraphicsBlock*>::iterator itr = d->m_processorMaps.begin();
	QList<Position> posList;
	for(; itr != d->m_processorMaps.end(); itr++)
	{
		ProcessorGraphicsBlock *p = itr.value();
		Position pos;
		pos.m_id	= p->processor()->id();
		pos.m_x		= p->scenePos().x();
		pos.m_y		= p->scenePos().y();
		posList.push_back(pos);
	}
	s.serialize(_T("Positions"), posList, _T("Position"));
	QList<GraphicsConnection> conList;
	QList<QGraphicsItem*> allItems = scene()->items();
	for (QList<QGraphicsItem*>::iterator itr = allItems.begin(); itr != allItems.end(); itr++)
	{
		if ((*itr)->type() == PortArrowLink::Type)
		{
			PortArrowLink* pItem = (PortArrowLink*)*itr;
			GraphicsConnection gc;
			gc.m_srcProcessor	 = pItem->getSrc()->processorId();
			gc.m_targetProcessor = pItem->getTarget()->processorId();
			gc.m_srcPort		 = pItem->getSrc()->index();
			gc.m_targetPort		 = pItem->getTarget()->index();
			gc.m_pt1 = pItem->getControlItemScenePosFirst();
			gc.m_pt2 = pItem->getControlItemScenePosSecond();
			conList.push_back(gc);
		}
		else if ((*itr)->type() == ProcessorArrowLink::Type)
		{
			ProcessorArrowLink* pItem = (ProcessorArrowLink*)*itr;
			GraphicsConnection gc;
			gc.m_srcProcessor	 = pItem->getSrc()->processor()->id();
			gc.m_targetProcessor = pItem->getTarget()->processor()->id();
			gc.m_pt1 = pItem->getControlItemScenePosFirst();
			gc.m_pt2 = pItem->getControlItemScenePosSecond();
			conList.push_back(gc);
			qDebug() << gc.m_pt1 << gc.m_pt2;
		}
	}
	s.serialize(_T("GraphicsConnectionList"), conList, "GraphicsConnection");
}

void DesignNetView::deserialize( Utils::XmlDeserializer &x )
{
	QList<Position> positions;
	QList<GraphicsConnection> conList;
	x.deserializeCollection(_T("Positions"), positions, _T("Position"));
	x.deserializeCollection(_T("GraphicsConnectionList"), conList, "GraphicsConnection");

	foreach(Processor *p, d->m_designnetSpace->processors())
	{
		foreach(Position pos, positions)
		{
			if (pos.m_id == p->id())
			{
				d->m_processorMaps.value(p)->setPos(pos.m_x, pos.m_y);
				break;
			}
		}
	}
	QList<QGraphicsItem*> allItems = scene()->items();
	for (QList<QGraphicsItem*>::iterator itr = allItems.begin(); itr != allItems.end(); itr++)
	{
		if ((*itr)->type() == PortArrowLink::Type)
		{
			PortArrowLink* pItem = (PortArrowLink*)*itr;
			for (QList<GraphicsConnection>::iterator c = conList.begin(); c != conList.end(); c++)
			{
				GraphicsConnection &con = *c;
				if (con.m_srcProcessor == pItem->getSrc()->processorId()
					&& con.m_targetProcessor == pItem->getTarget()->processorId()
					&& con.m_srcPort == pItem->getSrc()->index()
					&& con.m_targetPort == pItem->getTarget()->index())
				{
					pItem->setControlPointPos(con.m_pt1, true);
					pItem->setControlPointPos(con.m_pt2, false);
					break;
				}
			}
		}
		else if ((*itr)->type() == ProcessorArrowLink::Type)
		{
			ProcessorArrowLink* pItem = (ProcessorArrowLink*)*itr;
			for (QList<GraphicsConnection>::iterator c = conList.begin(); c != conList.end(); c++)
			{
				GraphicsConnection &con = *c;
				if (con.m_srcProcessor == pItem->getSrc()->processor()->id()
					&& con.m_targetProcessor == pItem->getTarget()->processor()->id()
					&& con.m_srcPort == -1
					&& con.m_targetPort == -1)
				{
					pItem->setControlPointPos(con.m_pt1, true);
					pItem->setControlPointPos(con.m_pt2, false);
					break;
				}
			}
		}
	}
}

void DesignNetView::OnShowMessage( const QString &strMessage )
{
	Core::ICore::messageManager()->printToOutputPanePopup(sender()->objectName() + ": " + strMessage);
}

void DesignNetView::onProcessorAdded(Processor* processor)
{
	ProcessorGraphicsBlock *pBlock = new ProcessorGraphicsBlock(processor, scene(), this);
	addProcessor(pBlock);
	pBlock->setFlag(QGraphicsItem::ItemIsMovable, m_eEditState == EditState_Move);
	scene()->addItem(pBlock);
}

void DesignNetView::onProcessorRemoved( Processor* processor )
{
	QMap<Processor*, ProcessorGraphicsBlock*>::Iterator itr = d->m_processorMaps.find(processor);
	if (itr != d->m_processorMaps.end())
	{
		ProcessorGraphicsBlock* p = itr.value();
		d->m_processorMaps.erase(itr);
		scene()->removeItem(p);
		delete p;
	}
}

void DesignNetView::onConnectionAdded(Processor* father, Processor* child)
{
	ProcessorArrowLink* pLink = 0;
	d->m_processorMaps[father]->setPortVisible(false);
	d->m_processorMaps[child]->setPortVisible(false);
	for (int i = 0; i < d->m_links.size(); i++)
	{
		if (d->m_links[i]->getSrc()->processor() == father && d->m_links[i]->getTarget()->processor() == child)
		{
			pLink = d->m_links[i];
			scene()->clearSelection();
			pLink->setSelected(true);
			return;
		}
	}

	if (pLink)
		return;

	pLink = new ProcessorArrowLink(0);

	scene()->clearSelection();
	scene()->addItem(pLink);
	pLink->connectProcessor(d->m_processorMaps[father], d->m_processorMaps[child]);
	pLink->setSelected(true);
	d->m_links.append(pLink);
}

void DesignNetView::onConnectionRemoved(Processor* father, Processor* child)
{
	ProcessorArrowLink* pLinkRemove = NULL;
	for (int i = 0; i < d->m_links.size(); i++)
	{
		ProcessorArrowLink* pLink = d->m_links[i];
		if (pLink->getSrc()->processor() == father && pLink->getTarget()->processor() == child)
		{
			pLinkRemove = pLink;
			delete pLink;
			break;
		}
	}
	if (pLinkRemove)
		d->m_links.removeOne(pLinkRemove);
}

void DesignNetView::onConnectionRemoved(Port* src, Port* target)
{

}

void DesignNetView::setEditState(EditState e)
{
	if (m_eEditState != e)
	{
		m_eEditState = e;
		foreach(ProcessorGraphicsBlock* pBlock, d->m_processorMaps.values())
			pBlock->setFlag(QGraphicsItem::ItemIsMovable, m_eEditState == EditState_Move);
		setCursor(m_eEditState == EditState_Link ? Qt::CrossCursor : Qt::ArrowCursor);
		Core::Command *pCmd = Core::ActionManager::command(m_eEditState == EditState_Link ? Constants::DESIGNNET_EDITSTATE_LINK_ACTION : Constants::DESIGNNET_EDITSTATE_MOVE_ACTION);
		pCmd->action()->setChecked(true);
	}
}

void DesignNetView::relayout()
{
	for (int i = 0; i < d->m_links.size(); i++)
	{
		ProcessorArrowLink* pLink = d->m_links.at(i);
		pLink->relayout();
	}
}

}
