#ifndef WHITEFOREST_HPP
#define WHITEFOREST_HPP

#include <Core/Global.hpp>
#include <QWidget>
#include <array>

class Profile5;
class ProfileDisplay5;
class DateEdit;
class QComboBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStandardItemModel;
class QTabWidget;
class TableView;

class WhiteForest final : public QWidget
{
    Q_OBJECT
signals:
    void profilesChanged(int);

public:
    WhiteForest(QWidget *parent = nullptr);
    bool hasProfiles() const;

public slots:
    void updateProfiles();

private:
    ProfileDisplay5 *profileDisplay;
    const Profile5 *currentProfile = nullptr;
    std::array<u8, 10> residents { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    QPushButton *residentButton;
    QPushButton *residentInfoButton;
    QTabWidget *tabs;
    QLineEdit *itemSeed;
    QSpinBox *itemInitial;
    QSpinBox *itemMax;
    std::array<QComboBox *, 10> generatorFilters;
    std::array<QComboBox *, 10> searcherFilters;
    TableView *generatorTable;
    QStandardItemModel *itemModel;
    QSpinBox *searcherInitial;
    QSpinBox *searcherMax;
    DateEdit *searcherStartDate;
    DateEdit *searcherEndDate;
    QPushButton *searchButton;
    QPushButton *cancelButton;
    QProgressBar *progressBar;
    TableView *searcherTable;
    QStandardItemModel *searcherModel;

    bool validateResidents(bool requireResident);
    void editResidents();
    void showResidentInfo();
    void updateItemFilters();
    void updateResidentText();
    void profileChanged(const Profile5 &profile);
    void generateItems();
    void searchItems();
    void goToGenerator();
};

#endif // WHITEFOREST_HPP
