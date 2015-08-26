/*
 * Copyright (C) 2008-2014 The QXmpp developers
 *
 * Author:
 *  Jeremy Lainé
 *
 * Source:
 *  https://github.com/qxmpp-project/qxmpp
 *
 * This file is a part of QXmpp library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <QDataStream>
#include <QDebug>

#include "QXmppRtcpPacket.h"

#define RTP_VERSION 2

enum DescriptionType {
    CnameType = 1,
    NameType  = 2
};

class QXmppRtcpPacketPrivate : public QSharedData
{
public:
    QXmppRtcpPacketPrivate();

    /// Number of report blocks.
    quint8 count;
    /// Payload type.
    quint8 type;
    /// Raw payload data.
    QByteArray payload;
    // Sender SSRC (for receiver / sender reports only).
    quint32 ssrc;

    QXmppRtcpSenderInfo senderInfo;
    QList<QXmppRtcpReceiverReport> receiverReports;
    QList<QXmppRtcpSourceDescription> sourceDescriptions;
};

class QXmppRtcpReceiverReportPrivate : public QSharedData
{
public:
    QXmppRtcpReceiverReportPrivate();
    bool read(QDataStream &stream);
    void write(QDataStream &stream) const;

    quint32 ssrc;
    quint8 fractionLost;
    quint32 cumulativeLost;
    quint32 highestSequence;
    quint32 jitter;
    quint32 srStamp;
    quint32 srDelay;
};

class QXmppRtcpSenderInfoPrivate : public QSharedData
{
public:
    QXmppRtcpSenderInfoPrivate();
    bool read(QDataStream &stream);
    void write(QDataStream &stream) const;

    quint64 ntpStamp;
    quint32 rtpStamp;
    quint32 packetCount;
    quint32 octetCount;
};

class QXmppRtcpSourceDescriptionPrivate : public QSharedData
{
public:
    QXmppRtcpSourceDescriptionPrivate();
    bool read(QDataStream &stream);
    void write(QDataStream &stream) const;

    quint32 ssrc;
    QString cname;
    QString name;
};

/// Constructs an empty RTCP packet

QXmppRtcpPacket::QXmppRtcpPacket()
    : d(new QXmppRtcpPacketPrivate())
{
}

/// Constructs a copy of other.
///
/// \param other

QXmppRtcpPacket::QXmppRtcpPacket(const QXmppRtcpPacket &other)
    : d(other.d)
{
}

QXmppRtcpPacket::~QXmppRtcpPacket()
{
}

/// Parses an RTCP packet.
///
/// \param ba
bool QXmppRtcpPacket::decode(const QByteArray &ba)
{
    QDataStream stream(ba);
    return read(stream);
}

/// Encodes an RTCP packet.

QByteArray QXmppRtcpPacket::encode() const
{
    QByteArray ba;
    ba.resize(4 + d->payload.size());

    QDataStream stream(&ba, QIODevice::WriteOnly);
    write(stream);
    return ba;
}

bool QXmppRtcpPacket::read(QDataStream &stream)
{
    quint8 tmp, type;
    quint16 len;

    // fixed header
    stream >> tmp;
    stream >> type;
    stream >> len;
    if (stream.status() != QDataStream::Ok)
        return false;

    // check version
    if ((tmp >> 6) != RTP_VERSION)
        return false;

    const int payloadLength = len << 2;
    d->count = (tmp & 0x1f);
    d->type = type;
    d->payload.resize(payloadLength);
    if (stream.readRawData(d->payload.data(), payloadLength) != payloadLength)
        return false;

    QDataStream s(d->payload);
    d->receiverReports.clear();
    d->senderInfo = QXmppRtcpSenderInfo();
    d->sourceDescriptions.clear();
    d->ssrc = 0;
    if (d->type == ReceiverReport || d->type == SenderReport) {
        s >> d->ssrc;
        if (d->type == SenderReport && !d->senderInfo.d->read(s))
            return false;
        for (int i = 0; i < d->count; ++i) {
            QXmppRtcpReceiverReport receiverReport;
            if (!receiverReport.d->read(s))
                return false;
            d->receiverReports << receiverReport;
        }
    } else if (d->type == SourceDescription) {
        for (int i = 0; i < d->count; ++i) {
            QXmppRtcpSourceDescription desc;
            if (!desc.d->read(s))
                return false;
            d->sourceDescriptions << desc;
        }
    }
    return true;
}

void QXmppRtcpPacket::write(QDataStream &stream) const
{
    QByteArray payload;
    quint8 count;

    QDataStream s(&payload, QIODevice::WriteOnly);
    if (d->type == ReceiverReport || d->type == SenderReport) {
        count = d->receiverReports.size();
        s << d->ssrc;
        if (d->type == SenderReport)
            d->senderInfo.d->write(s);
        foreach (const QXmppRtcpReceiverReport &report, d->receiverReports)
            report.d->write(s);
    } else if (d->type == SourceDescription) {
        count = d->sourceDescriptions.size();
        foreach (const QXmppRtcpSourceDescription &desc, d->sourceDescriptions)
            desc.d->write(s);
    } else {
        count = d->count;
        payload = d->payload;
    }

    stream << quint8((RTP_VERSION << 6) | (count & 0x1f));
    stream << d->type;
    stream << quint16(payload.size() >> 2);
    stream.writeRawData(payload.constData(), payload.size());
}

QList<QXmppRtcpReceiverReport> QXmppRtcpPacket::receiverReports() const
{
    return d->receiverReports;
}

void QXmppRtcpPacket::setReceiverReports(const QList<QXmppRtcpReceiverReport> &reports)
{
    d->receiverReports = reports;
}

QXmppRtcpSenderInfo QXmppRtcpPacket::senderInfo() const
{
    return d->senderInfo;
}

void QXmppRtcpPacket::setSenderInfo(const QXmppRtcpSenderInfo &senderInfo)
{
    d->senderInfo = senderInfo;
}

QList<QXmppRtcpSourceDescription> QXmppRtcpPacket::sourceDescriptions() const
{
    return d->sourceDescriptions;
}

void QXmppRtcpPacket::setSourceDescriptions(const QList<QXmppRtcpSourceDescription> &descriptions)
{
    d->sourceDescriptions = descriptions;
}

/// Returns the RTCP packet's source SSRC.
///
/// This is only applicable for Sender Reports or Receiver Reports.

quint32 QXmppRtcpPacket::ssrc() const
{
    return d->ssrc;
}

/// Sets the RTCP packet's source SSRC.
///
/// This is only applicable for Sender Reports or Receiver Reports.

void QXmppRtcpPacket::setSsrc(quint32 ssrc)
{
    d->ssrc = ssrc;
}

/// Returns the RTCP packet type.

quint8 QXmppRtcpPacket::type() const
{
    return d->type;
}

/// Sets the RTCP packet type.
///
/// \param type

void QXmppRtcpPacket::setType(quint8 type)
{
    d->type = type;
}

QXmppRtcpPacketPrivate::QXmppRtcpPacketPrivate()
    : count(0)
    , type(0)
    , ssrc(0)
{
}

/// Constructs an empty receiver report.

QXmppRtcpReceiverReport::QXmppRtcpReceiverReport()
    : d(new QXmppRtcpReceiverReportPrivate())
{
}

/// Constructs a copy of other.
///
/// \param other

QXmppRtcpReceiverReport::QXmppRtcpReceiverReport(const QXmppRtcpReceiverReport &other)
    : d(other.d)
{
}

QXmppRtcpReceiverReport::~QXmppRtcpReceiverReport()
{
}

quint32 QXmppRtcpReceiverReport::ssrc() const
{
    return d->ssrc;
}

void QXmppRtcpReceiverReport::setSsrc(quint32 ssrc)
{
    d->ssrc = ssrc;
}

QXmppRtcpReceiverReportPrivate::QXmppRtcpReceiverReportPrivate()
    : ssrc(0)
    , fractionLost(0)
    , cumulativeLost(0)
    , highestSequence(0)
    , jitter(0)
    , srStamp(0)
    , srDelay(0)
{
}

bool QXmppRtcpReceiverReportPrivate::read(QDataStream &stream)
{
    quint32 tmp;
    stream >> ssrc;
    stream >> tmp;
    fractionLost = (tmp >> 24) & 0xff;
    cumulativeLost = tmp & 0xffffff;
    stream >> highestSequence;
    stream >> jitter;
    stream >> srStamp;
    stream >> srDelay;
    return stream.status() == QDataStream::Ok;
}

void QXmppRtcpReceiverReportPrivate::write(QDataStream &stream) const
{
    stream << ssrc;
    stream << quint32((fractionLost << 24) | cumulativeLost);
    stream << highestSequence;
    stream << jitter;
    stream << srStamp;
    stream << srDelay;
}

/// Constructs an empty sender report.

QXmppRtcpSenderInfo::QXmppRtcpSenderInfo()
    : d(new QXmppRtcpSenderInfoPrivate())
{
}

/// Constructs a copy of other.
///
/// \param other

QXmppRtcpSenderInfo::QXmppRtcpSenderInfo(const QXmppRtcpSenderInfo &other)
    : d(other.d)
{
}

QXmppRtcpSenderInfo::~QXmppRtcpSenderInfo()
{
}

quint64 QXmppRtcpSenderInfo::ntpStamp() const
{
    return d->ntpStamp;
}

void QXmppRtcpSenderInfo::setNtpStamp(quint64 ntpStamp)
{
    d->ntpStamp = ntpStamp;
}

quint32 QXmppRtcpSenderInfo::rtpStamp() const
{
    return d->rtpStamp;
}

void QXmppRtcpSenderInfo::setRtpStamp(quint32 rtpStamp)
{
    d->rtpStamp = rtpStamp;
}

quint32 QXmppRtcpSenderInfo::octetCount() const
{
    return d->octetCount;
}

void QXmppRtcpSenderInfo::setOctetCount(quint32 count)
{
    d->octetCount = count;
}

quint32 QXmppRtcpSenderInfo::packetCount() const
{
    return d->packetCount;
}

void QXmppRtcpSenderInfo::setPacketCount(quint32 count)
{
    d->packetCount = count;
}

QXmppRtcpSenderInfoPrivate::QXmppRtcpSenderInfoPrivate()
    : ntpStamp(0)
    , rtpStamp(0)
    , packetCount(0)
    , octetCount(0)
{
}

bool QXmppRtcpSenderInfoPrivate::read(QDataStream &stream)
{
    stream >> ntpStamp;
    stream >> rtpStamp;
    stream >> packetCount;
    stream >> octetCount;
    return stream.status() == QDataStream::Ok;
}

void QXmppRtcpSenderInfoPrivate::write(QDataStream &stream) const
{
    stream << ntpStamp;
    stream << rtpStamp;
    stream << packetCount;
    stream << octetCount;
}

/// Constructs an empty source description

QXmppRtcpSourceDescription::QXmppRtcpSourceDescription()
    : d(new QXmppRtcpSourceDescriptionPrivate())
{
}

/// Constructs a copy of other.
///
/// \param other

QXmppRtcpSourceDescription::QXmppRtcpSourceDescription(const QXmppRtcpSourceDescription &other)
    : d(other.d)
{
}

QXmppRtcpSourceDescription::~QXmppRtcpSourceDescription()
{
}

QString QXmppRtcpSourceDescription::cname() const
{
    return d->cname;
}

void QXmppRtcpSourceDescription::setCname(const QString &cname)
{
    d->cname = cname;
}

QString QXmppRtcpSourceDescription::name() const
{
    return d->name;
}

void QXmppRtcpSourceDescription::setName(const QString &name)
{
    d->name = name;
}

quint32 QXmppRtcpSourceDescription::ssrc() const
{
    return d->ssrc;
}

void QXmppRtcpSourceDescription::setSsrc(quint32 ssrc)
{
    d->ssrc = ssrc;
}

QXmppRtcpSourceDescriptionPrivate::QXmppRtcpSourceDescriptionPrivate()
    : ssrc(0)
{
}

bool QXmppRtcpSourceDescriptionPrivate::read(QDataStream &stream)
{
    QByteArray buffer;
    quint8 itemType, itemLength;
    quint16 chunkLength = 0;

    stream >> ssrc;
    if (stream.status() != QDataStream::Ok)
        return false;
    while (true) {
        stream >> itemType;
        if (stream.status() != QDataStream::Ok)
            return false;
        if (!itemType) {
            chunkLength++;
            break;
        }

        stream >> itemLength;
        if (stream.status() != QDataStream::Ok)
            return false;

        buffer.resize(itemLength);
        if (stream.readRawData(buffer.data(), itemLength) != itemLength)
            return false;
        chunkLength += itemLength + 2;

        if (itemType == CnameType)
            cname = QString::fromUtf8(buffer);
        else if (itemType == NameType)
            name = QString::fromUtf8(buffer);
    }
    if (chunkLength % 4) {
        buffer.resize(4 - chunkLength % 4);
        if (stream.readRawData(buffer.data(), buffer.size()) != buffer.size())
            return false;
        if (buffer != QByteArray(buffer.size(), '\0'))
            return false;
    }
    return true;
}

void QXmppRtcpSourceDescriptionPrivate::write(QDataStream &stream) const
{
    QByteArray buffer;
    quint16 chunkLength = 0;

    stream << ssrc;
    if (!cname.isEmpty()) {
        buffer = cname.toUtf8();
        stream << quint8(CnameType);
        stream << quint8(buffer.size());
        stream.writeRawData(buffer.constData(), buffer.size());
        chunkLength += 2 + buffer.size();
    }
    if (!name.isEmpty()) {
        buffer = name.toUtf8();
        stream << quint8(NameType);
        stream << quint8(buffer.size());
        stream.writeRawData(buffer.constData(), buffer.size());
        chunkLength += 2 + buffer.size();
    }
    stream << quint8(0);
    chunkLength++;
    if (chunkLength % 4) {
        buffer = QByteArray(4 - chunkLength % 4, '\0');
        stream.writeRawData(buffer.constData(), buffer.size());
    }
}
