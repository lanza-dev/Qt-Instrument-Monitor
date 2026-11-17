#ifndef BATCHWORKER_H
#define BATCHWORKER_H

#pragma once

#include <QList>
#include <QObject>
#include <QThread>

#include <algorithm>

class BatchWorker : public QObject
{
    Q_OBJECT

public:
    explicit BatchWorker(const QList<double> &snapshot)
        : samples(snapshot)
    {
    }

    void analyze()
    {
        double minimum = samples.first();
        double maximum = minimum;
        double sum = 0.0;

        for (double ppm : samples)
        {
            if (QThread::currentThread()->isInterruptionRequested())
            {
                emit finished();
                return;
            }

            //QThread::msleep(100);

            minimum = std::min(minimum, ppm);
            maximum = std::max(maximum, ppm);
            sum += ppm;
        }

        emit resultReady(samples.size(), minimum, maximum,
                         sum / samples.size());
        emit finished();
    }

signals:
    void resultReady(qsizetype count, double minimum,
                     double maximum, double average);
    void finished();

private:
    const QList<double> samples;
};

#endif // BATCHWORKER_H
