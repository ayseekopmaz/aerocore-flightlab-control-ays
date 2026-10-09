#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

// Asynchronous, read-only discovery. It never installs WSL or clones PX4.
class EnvironmentDiscovery final : public QObject {
    Q_OBJECT
public:
    explicit EnvironmentDiscovery(QObject *parent=nullptr);
    ~EnvironmentDiscovery() override;
    void start();
    bool busy() const { return stage_!=Stage::Idle; }
    static QString chooseDistribution(const QByteArray &wslOutput);
signals:
    void found(const QString &distribution,const QString &repository);
    void message(const QString &text);
    void busyChanged(bool busy);
private:
    enum class Stage { Idle, List, Locate };
    Stage stage_=Stage::Idle;
    QProcess process_;
    QTimer timeout_;
    QString distribution_;
    void finish(const QString &error={});
};
