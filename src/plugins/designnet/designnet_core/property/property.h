﻿#pragma once
#include <QObject>
#include "../designnet_core_def.h"
#include "aggregation/aggregate.h"
#include "Utils/XML/xmldeserializer.h"
#include "Utils/XML/xmlserializable.h"
#include "Utils/XML/xmlserializer.h"


#define DECLARE_PROPERTY_TYPE(x) \
	virtual DesignNet::PropertyType propertyType() const { return x; }

namespace Utils{
class XmlSerializer;
}
namespace DesignNet{

class PropertyOwner;
class DESIGNNET_CORE_EXPORT Property : public QObject, public Utils::XmlSerializable
{
    Q_OBJECT

public:

    /*!
     * \brief Property 构造函数
     * \param[in] id 属性唯一ID（在一个PropertyOwner中是唯一的）
     * \param[in] name 属性名，将会在GUI中显示出来
     * \param[in] parent
     */
    explicit Property(const QString &id, const QString &name, QObject *parent = 0);
	virtual ~Property() {}

    /*!
     * \brief typeID 返回该属性的类型ID
     * \note 属性的typeID与id不同，id是该属性在它所属的Owner中的唯一标识，
     * 而属性typeID标识了这个属性到底是什么类型的。
     */

    QString id() const;
    void	setId(const QString &id);

    QString name() const;
    void	setName(const QString &name);

    PropertyOwner *owner() const;
    void	setOwner(PropertyOwner *owner);

	virtual DesignNet::PropertyType propertyType() const = 0;

	//<! 序列化相关代码
	void	serialize(Utils::XmlSerializer& s) const;
	void	deserialize(Utils::XmlDeserializer& s);
	QString serializableType() const;
	Utils::XmlSerializable* createSerializable() const;

signals:

    void changed();

public slots:

protected:

    QString					m_id;
    QString					m_name;
    PropertyOwner*			m_owner;
};
}
