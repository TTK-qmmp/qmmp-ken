#include "decoderkenfactory.h"
#include "decoder_ken.h"
#include "kenhelper.h"

#include <QMessageBox>

bool DecoderKenFactory::canDecode(QIODevice *input) const
{
    const QFile * const file = qobject_cast<QFile*>(input);
    if(!file)
    {
        return false;
    }

    KenHelper helper(file->fileName());
    return helper.initialize();
}

DecoderProperties DecoderKenFactory::properties() const
{
    DecoderProperties properties;
    properties.name = tr("Ken Plugin");
    properties.shortName = "ken";
    properties.filters << "*.kdm" << "*.ksm" << "*.sm" << "*.snd";
    properties.description = "Ken Silverman's Audio File";
    properties.protocols << "file";
    properties.priority = 8;
    properties.noInput = true;
    return properties;
}

Decoder *DecoderKenFactory::create(const QString &path, QIODevice *input)
{
    Q_UNUSED(input);
    return new DecoderKen(path);
}

QList<TrackInfo*> DecoderKenFactory::createPlayList(const QString &path, TrackInfo::Parts parts, QStringList *)
{
    TrackInfo *info = new TrackInfo(path);
    if(parts == TrackInfo::Parts())
    {
        return QList<TrackInfo*>() << info;
    }

    KenHelper helper(path);
    if(!helper.initialize())
    {
        delete info;
        return QList<TrackInfo*>();
    }

    if(parts & TrackInfo::Properties)
    {
        info->setValue(Qmmp::BITRATE, helper.bitrate());
        info->setValue(Qmmp::SAMPLERATE, helper.sampleRate());
        info->setValue(Qmmp::CHANNELS, helper.channels());
        info->setValue(Qmmp::BITS_PER_SAMPLE, helper.depth());
        info->setValue(Qmmp::FORMAT_NAME, "Ken Silverman");
        info->setDuration(helper.totalTime());
    }
    return QList<TrackInfo*>() << info;
}

MetaDataModel* DecoderKenFactory::createMetaDataModel(const QString &path, bool readOnly)
{
    Q_UNUSED(path);
    Q_UNUSED(readOnly);
    return nullptr;
}

#if (QMMP_VERSION_INT < 0x10700) || (0x20000 <= QMMP_VERSION_INT && QMMP_VERSION_INT < 0x20200)
void DecoderKenFactory::showSettings(QWidget *parent)
{
    Q_UNUSED(parent);
}
#else
QDialog *DecoderKenFactory::createSettings(QWidget *parent)
{
    Q_UNUSED(parent);
    return nullptr;
}
#endif

void DecoderKenFactory::showAbout(QWidget *parent)
{
    QMessageBox::about(parent, tr("About Ken Reader Plugin"),
                       tr("Qmmp Ken Reader Plugin") + "\n" +
                       tr("Written by: Greedysky <greedysky@163.com>"));
}

QString DecoderKenFactory::translation() const
{
    return QString();
}
