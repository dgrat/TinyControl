#include "rcwidget.h"
#include <cassert>


void QRCWidget::sl_setRadioEnabled(bool state) {
    m_bRadioEnabled = state;
}

QRCWidget::QRCWidget(QUdpSocket *pSock, QSerialPort *pSerialPort, QWidget *parent) : QAbsFrame(parent) {
    assert(pSock != NULL);
    m_pUdpSock = pSock;

    m_bThrottleHold = false;

    m_pSerialPort = pSerialPort;

    this->setMinimumSize(480, 480);

    m_bRadioEnabled = true;
    m_bAltitudeHold = false;

    m_iUpdateTime = 5;
    m_fTimeConstRed = (float)m_iUpdateTime/150.f;
    m_fTimeConstEnh = (float)m_iUpdateTime/75.f;

    connect(&m_caliEventTimer, SIGNAL(timeout() ), this, SLOT(sl_customKeyPressHandler() ) );
    connect(&m_keyEventTimer, SIGNAL(timeout() ), this, SLOT(sl_gamepadHandler() ) );
    connect(&m_keyEventTimer, SIGNAL(timeout() ), this, SLOT(sl_customKeyPressHandler() ) );
    connect(&m_keyEventTimer, SIGNAL(timeout() ), this, SLOT(sl_customKeyReleaseHandler() ) );

    connect(&m_udpSendTimer, SIGNAL(timeout() ), this, SLOT(sl_sendRC2UDP() ) );
    connect(&m_trimTimer, SIGNAL(timeout() ), this, SLOT(sl_sendTrim2UDP() ) );
}

void QRCWidget::start() {
    m_trimTimer.start(500);
    m_udpSendTimer.start(20);
    m_keyEventTimer.start(m_iUpdateTime);
}

void QRCWidget::stop() {
    m_trimTimer.stop();
    m_udpSendTimer.stop();
    m_keyEventTimer.stop();
}

int CUSTOM_KEY::mapCustomKeyIndex(const int key) {
    if(key >= 0x01000000) {
        return 256 + (key ^ 0x01000000);
    } else {
        return key;
    }
}

void QRCWidget::initGyro2UDP() {
    if(m_InputByUser.THR > m_InputRange.THR_MIN) {
        qDebug() << "Gyrometer calibration failed, because throttle is too high";
        return;
    }

    // Calibration will go over 6 edges of the board
    static int iStep = 6;
    static bool bInit = false;
    
    QString com = "";
    com.append("{\"t\":\"gyr\",\"cal\":"); 
    com.append(QString::number(true) ); 
    com.append("}");
    
    QString usr = "";
    usr.append("{\"t\":\"user_interactant\"}"); ;
    
    // Init calibration
    if(iStep >= 6) {
        this->stop();
        qDebug() << "Try to start gyrometer calibration";
        sendJSON2UDP(com, false);
        
        m_caliTime.start();
        m_caliEventTimer.start(10);
        bInit = true;
        return;
    }
    // Run through user interactant
    if(bInit) {
        if(m_caliTime.elapsed() < 250) {
            return;
        }
        sendJSON2UDP(usr, false);
        if(!iStep--) {
            iStep = 6;
            bInit = false;
            m_caliEventTimer.stop();
            this->start();
        }
    }
    
    m_caliTime.restart();
}

void QRCWidget::activAltihold2UDP() {
    QString com = "";
    com.append("{\"t\":\"uav\",\"lat_d\":");
    com.append(QString::number(0) );
    com.append(",\"lon_d\":");
    com.append(QString::number(0) );
    com.append(",\"alt_m\":");
    com.append(QString::number(0) );
    com.append(",\"flag_t\":");
    com.append(QString::number(1 << 1) ); // HLD_ALTITUDE_F
    com.append("}");

    qDebug() << "Try to init altitude hold";
    sendJSON2UDP(com, false);
}

void QRCWidget::deactAltihold2UDP() {
    QString com = "";
    com.append("{\"t\":\"uav\",\"lat_d\":");
    com.append(QString::number(0) );
    com.append(",\"lon_d\":");
    com.append(QString::number(0) );
    com.append(",\"alt_m\":");
    com.append(QString::number(0) );
    com.append(",\"flag_t\":");
    com.append(QString::number(1 << 0) ); // NOTHING_F
    com.append("}");

    qDebug() << "Try to init normal mode";
    sendJSON2UDP(com, false);
}

void QRCWidget::sl_setAttitudeCorr(float fRoll, float fPitch) {
    m_DRIFT.PIT = fPitch;
    m_DRIFT.ROL = fRoll;
    sendJSON2UDP(m_DRIFT.str_makeWiFiCommand(), false);
}

void QRCWidget::emitCTRLChanges() {
    emit si_thrChanged(m_InputByUser.THR);
    emit si_rolChanged(m_InputByUser.ROL);
    emit si_pitChanged(m_InputByUser.PIT);
    emit si_yawChanged(m_InputByUser.YAW);
}


void QRCWidget::sl_gamepadHandler() {
    // Most important safety switch
    if(!m_bDeviceArmed && m_bGamepadConnected) {
        m_InputByUser.PIT = 0;
        m_InputByUser.ROL = 0;
        m_InputByUser.YAW = 0;
        m_InputByUser.THR = m_InputRange.THR_MIN;

        emitCTRLChanges();
        return;
    }

    // Gamepad stats changed?
    if(!m_bGamepadInUse) {
        return;
    }

    m_InputByUser.PIT = m_InputRange.PIT_MAX * m_fStickLY;
    m_InputByUser.ROL = m_InputRange.ROL_MAX * m_fStickLX;
    m_InputByUser.YAW = m_InputRange.YAW_MAX * -m_fStickRX;
/*
    float fYawR2 = m_InputRange.YAW_MAX * m_fStickR2;
    float fYawL2 = m_InputRange.YAW_MAX * -m_fStickL2;

    if(abs(fYawR2) && fYawR2 > m_InputByUser.YAW) {
        m_InputByUser.YAW = fYawR2;
    }
    if(abs(fYawL2) && fYawL2 < m_InputByUser.YAW) {
        m_InputByUser.YAW = fYawL2;
    }
*/
    float fStickRY = -m_fStickRY;
    if(fStickRY > 0) {
        int iThrP = (m_InputRange.THR_80P - m_InputRange.THR_MIN) * fStickRY + m_InputRange.THR_MIN;
        if(iThrP > m_InputByUser.THR) {
            m_InputByUser.THR = iThrP;
        }
    }
    if(fStickRY < 0) {
        int iThrN = (m_InputRange.THR_80P - m_InputRange.THR_MIN) * (1 + fStickRY) + m_InputRange.THR_MIN;
        if(iThrN < m_InputByUser.THR) {
            m_InputByUser.THR = iThrN;
        }
    }

    QWidget::update();
    emitCTRLChanges();
}

void QRCWidget::sl_customKeyPressHandler() {
    if( m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_Backspace)] == true ||
        (!m_bDeviceArmed && m_bGamepadConnected) )
    {
        m_InputByUser.PIT = 0;
        m_InputByUser.ROL = 0;
        m_InputByUser.YAW = 0;
        m_InputByUser.THR = m_InputRange.THR_MIN;

        emitCTRLChanges();
        return;
    }

    // Inertial calibration
    if(m_customKeyStatus[Qt::Key_C] == true) {
        qDebug() << "C";
        initGyro2UDP();
        m_customKeyStatus[Qt::Key_C] = false;
    }

    if(m_customKeyStatus[Qt::Key_H] == true) {
        qDebug() << "H";
        if(!m_bAltitudeHold) {
            activAltihold2UDP();
            m_bAltitudeHold = true;
        }
    }

    if(m_customKeyStatus[Qt::Key_W] == true && !m_bGamepadInUse) {
        qDebug() << "W";
        if(m_InputByUser.PIT > 0) {
            m_InputByUser.PIT = 0;
        }

        if(m_InputByUser.PIT + m_InputRange.PIT_MIN/10 >= m_InputRange.PIT_MIN)
            m_InputByUser.PIT += m_InputRange.PIT_MIN/10 * m_fTimeConstEnh;
        else m_InputByUser.PIT = m_InputRange.PIT_MIN;
    }
    if(m_customKeyStatus[Qt::Key_S] == true && !m_bGamepadInUse) {
        qDebug() << "S";
        if(m_InputByUser.PIT < 0) {
            m_InputByUser.PIT = 0;
        }

        if(m_InputByUser.PIT + m_InputRange.PIT_MAX/10 <= m_InputRange.PIT_MAX)
            m_InputByUser.PIT += m_InputRange.PIT_MAX/10 * m_fTimeConstEnh;
        else m_InputByUser.PIT = m_InputRange.PIT_MAX;
    }

    if(m_customKeyStatus[Qt::Key_A] == true && !m_bGamepadInUse) {
        qDebug() << "A";
        if(m_InputByUser.ROL > 0) {
            m_InputByUser.ROL = 0;
        }

        if(m_InputByUser.ROL + m_InputRange.ROL_MIN/10 >= m_InputRange.ROL_MIN)
            m_InputByUser.ROL += m_InputRange.ROL_MIN/10 * m_fTimeConstEnh;
        else m_InputByUser.ROL = m_InputRange.ROL_MIN;
    }
    if(m_customKeyStatus[Qt::Key_D] == true && !m_bGamepadInUse) {
        qDebug() << "D";
        if(m_InputByUser.ROL < 0) {
            m_InputByUser.ROL = 0;
        }

        if(m_InputByUser.ROL + m_InputRange.ROL_MAX/10 <= m_InputRange.ROL_MAX)
            m_InputByUser.ROL += m_InputRange.ROL_MAX/10 * m_fTimeConstEnh;
        else m_InputByUser.ROL = m_InputRange.ROL_MAX;
    }

    if(m_customKeyStatus[Qt::Key_Q] == true && !m_bGamepadInUse) {
        qDebug() << "Q";
        if(m_InputByUser.YAW < 0) {
            m_InputByUser.YAW = 0;
        }

        if(m_InputByUser.YAW + m_InputRange.YAW_MAX/10 <= m_InputRange.YAW_MAX)
            m_InputByUser.YAW += m_InputRange.YAW_MAX/10 * m_fTimeConstEnh;
        else m_InputByUser.YAW = m_InputRange.YAW_MAX;
    }
    if(m_customKeyStatus[Qt::Key_E] == true && !m_bGamepadInUse) {
        qDebug() << "E";
        if(m_InputByUser.YAW > 0) {
            m_InputByUser.YAW = 0;
        }

        if(m_InputByUser.YAW + m_InputRange.YAW_MIN/10 >= m_InputRange.YAW_MIN)
            m_InputByUser.YAW += m_InputRange.YAW_MIN/10 * m_fTimeConstEnh;
        else m_InputByUser.YAW = m_InputRange.YAW_MIN;
    }

    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_F1)] == true) {
        m_InputRange.setF1();
    }
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_F2)] == true) {
        m_InputRange.setF2();
    }
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_F3)] == true) {
        m_InputRange.setF3();
    }
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_F4)] == true) {
        m_InputRange.setF4();
    }

    // Quadro moves to front
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_8)] == true) {
        m_DRIFT.PIT -= 0.05 * m_fTimeConstEnh;
        sendJSON2UDP(m_DRIFT.str_makeWiFiCommand(), false);
        
        emit si_attitudeCorrChanged(m_DRIFT.ROL, m_DRIFT.PIT);
        qDebug() << "Drift correction: Pitch=" << m_DRIFT.str_makeWiFiCommand();
    }
    // Quadro moves backwards
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_2)] == true) {
        m_DRIFT.PIT += 0.05 * m_fTimeConstEnh;
        sendJSON2UDP(m_DRIFT.str_makeWiFiCommand(), false);
        
        emit si_attitudeCorrChanged(m_DRIFT.ROL, m_DRIFT.PIT);
        qDebug() << "Drift correction: Pitch=" << m_DRIFT.str_makeWiFiCommand();
    }
    // Quadro moves to the left
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_4)] == true) {
        m_DRIFT.ROL -= 0.05 * m_fTimeConstEnh;
        sendJSON2UDP(m_DRIFT.str_makeWiFiCommand(), false);
        
        emit si_attitudeCorrChanged(m_DRIFT.ROL, m_DRIFT.PIT);
        qDebug() << "Drift correction: Roll=" << m_DRIFT.str_makeWiFiCommand();
    }
    // // Quadro moves to the right
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_6)] == true) {
        m_DRIFT.ROL += 0.05 * m_fTimeConstEnh;
        sendJSON2UDP(m_DRIFT.str_makeWiFiCommand(), false);
        
        emit si_attitudeCorrChanged(m_DRIFT.ROL, m_DRIFT.PIT);
        qDebug() << "Drift correction: Roll=" << m_DRIFT.str_makeWiFiCommand();
    }

    float fStep = 1.35;
    float fAccelTime_ms = 50.f;
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_Up)] == true && !m_bGamepadInUse) {
        m_tThrottleRed.restart();
        if(!m_bThrottleHold) {
            m_tThrottleEnh.restart();
            m_bThrottleHold = true;
        }
        float fMod = (float)m_tThrottleEnh.elapsed() / fAccelTime_ms;
        //qDebug() << "Enh: " << fMod;

        if(m_InputByUser.THR + fStep <= m_InputRange.THR_80P)
            m_InputByUser.THR += fStep * m_fTimeConstEnh * (1.0 + fMod);
        else m_InputByUser.THR = m_InputRange.THR_80P;
    }
    if(m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_Down)] == true && !m_bGamepadInUse) {
        m_tThrottleEnh.restart();
        if(!m_bThrottleHold) {
            m_tThrottleRed.restart();
            m_bThrottleHold = true;
        }
        float fMod = (float)m_tThrottleRed.elapsed() / fAccelTime_ms;
        //qDebug() << "Red:" << fMod;

        if(m_InputByUser.THR - fStep >= m_InputRange.THR_MIN)
            m_InputByUser.THR -= fStep * m_fTimeConstEnh * (1.0 + fMod);
        else m_InputByUser.THR = m_InputRange.THR_MIN;
    }

    QWidget::update();
    emitCTRLChanges();
}

void QRCWidget::sl_sendTrim2UDP() {
    sendJSON2UDP(m_DRIFT.str_makeWiFiCommand(), false);
}

void QRCWidget::sl_customKeyReleaseHandler() {
    if(m_bGamepadInUse) {
        return;
    }

    if(!m_customKeyStatus[Qt::Key_W] && !m_customKeyStatus[Qt::Key_S]) {
        m_InputByUser.PIT > 0 ? m_InputByUser.PIT -= 1 * m_fTimeConstRed : m_InputByUser.PIT += 1 * m_fTimeConstRed;
    }

    if(!m_customKeyStatus[Qt::Key_A] && !m_customKeyStatus[Qt::Key_D]) {
        m_InputByUser.ROL > 0 ? m_InputByUser.ROL -= 1 * m_fTimeConstRed : m_InputByUser.ROL += 1 * m_fTimeConstRed;
    }

    if(!m_customKeyStatus[Qt::Key_Q] && !m_customKeyStatus[Qt::Key_E]) {
        m_InputByUser.YAW > 0 ? m_InputByUser.YAW -= 1 * m_fTimeConstRed : m_InputByUser.YAW += 1 * m_fTimeConstRed;
    }

    if(m_bAltitudeHold) {
        if(!m_customKeyStatus[Qt::Key_H]) {
            deactAltihold2UDP();
            m_bAltitudeHold = false;
        }
    }

    if(!m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_Up)] == true) {
        m_tThrottleEnh.restart();
    }
    if(!m_customKeyStatus[CUSTOM_KEY::mapCustomKeyIndex(Qt::Key_Down)] == true) {
        m_tThrottleRed.restart();
    }
}

void QRCWidget::sendJSON2UDP(QString sCom, bool isCommand) {
    if(!m_pUdpSock)
        return;

    if (sCom.size() > 0) {
        //qDebug() << "WiFi: " << sCom;
        m_pUdpSock->write(sCom.toLatin1(), sCom.size() );
        if(isCommand) {
            emit si_send2Model(sCom, "command");
        }
        else {
            emit si_send2Model(sCom, "option");
        }
    }
}

void QRCWidget::sendJSON2COM(QString sCom, bool isCommand) {
    if(!m_pUdpSock)
        return;

    if (sCom.size() > 0) {
        //qDebug() << "WiFi: " << sCom;
        m_pSerialPort->write(sCom.toLatin1(), sCom.size() );
        if(isCommand) {
            emit si_send2Model(sCom, "command");
        }
        else {
            emit si_send2Model(sCom, "option");
        }
    }
}

void QRCWidget::sendJSON2COM(QPair<int, char*> pair) {
    if(!m_pSerialPort || !m_pSerialPort->isOpen() )
        return;

    if (pair.first > 0) {
        qDebug() << "Radio: " << pair.first << pair.second;
        m_pSerialPort->write(pair.second, pair.first);
    }
}

void QRCWidget::sl_sendRC2UDP() {
    sendJSON2UDP(m_InputByUser.str_makeWiFiCommand() );
    if(m_bRadioEnabled) {
       sendJSON2COM(m_InputByUser.cstr_makeRadioCommand() );
    }
}
