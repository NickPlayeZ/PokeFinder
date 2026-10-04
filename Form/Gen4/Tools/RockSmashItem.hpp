#ifndef ROCKSMASHITEM_HPP
#define ROCKSMASHITEM_HPP

#include <Core/Global.hpp>
#include <QWidget>
#include <vector>

class Profile4;
class ProfileDisplay4;
class ComboBox;
class ComboMenu;
class QCheckBox;
class QProgressBar;
class QPushButton;
class QSpinBox;
class TableView;
class TextBox;

class RockSmashItem final : public QWidget
{
    Q_OBJECT
signals:
    void profilesChanged(int);
    void openGenerator(const Profile4 &profile, u8 location, u32 seed, u8 lead, bool rockSmashPokemon,
                       const std::vector<u32> &targetAdvances);

public:
    explicit RockSmashItem(QWidget *parent = nullptr);
    ~RockSmashItem() override;
    bool hasProfiles() const;

public slots:
    void updateProfiles();

private slots:
    void profileChanged(const Profile4 &profile);
    void locationChanged(int index);
    void search();
    void seedToTime();
    void openInGenerator();

private:
    ProfileDisplay4 *profileDisplay;
    TextBox *minDelay;
    TextBox *maxDelay;
    TextBox *minAdvance;
    TextBox *maxAdvance;
    ComboBox *location;
    ComboBox *item;
    ComboMenu *lead;
    QCheckBox *rockSmashPokemon;
    QSpinBox *amount;
    QPushButton *searchButton;
    QPushButton *cancelButton;
    QProgressBar *progressBar;
    TableView *table;
    const Profile4 *currentProfile;
};

#endif // ROCKSMASHITEM_HPP
