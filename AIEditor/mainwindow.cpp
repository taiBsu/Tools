#include <iostream>

#include <QtGui>
#include <QFileDialog>
#include <QSettings>
#include <QList>
#include <QMap>

#include "mainwindow.h"
#include "treemodel.h"
#include "node.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi(this);

    loadSettings();

    currentFileInfo.setFile(scriptsDir.absolutePath() + tr("/templates/example.lua"));

    TreeModel *btModel = new TreeModel();
    TreeModel *dtModel = new TreeModel();
    btTreeView->setModel(btModel);
    dtTreeView->setModel(dtModel);
    btTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    dtTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    
    connect(btTreeView->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this,
            SLOT(btSelectionCallback(const QItemSelection&, const QItemSelection&)));
    connect(dtTreeView->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this,
            SLOT(dtSelectionCallback(const QItemSelection&, const QItemSelection&)));

    connect(btModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(idChangedCallback(const QModelIndex&, const QModelIndex&)));
    connect(dtModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(idChangedCallback(const QModelIndex&, const QModelIndex&)));

    // File Menu
    connect(actionOpen, SIGNAL(triggered()), this, SLOT(openFileDialog()));
    connect(actionSelect_Scripts, SIGNAL(triggered()), this, SLOT(openDirDialog()));
    connect(actionSave_As, SIGNAL(triggered()), this, SLOT(openSaveDialog()));
    connect(actionExit, SIGNAL(triggered()), qApp, SLOT(quit()));

    // Actions Menu
    connect(menuActions, SIGNAL(aboutToShow()), this, SLOT(updateBehaviors()));
    connect(actionSelector, SIGNAL(triggered()), this, SLOT(insertChild()));
    compositeGroup.addAction(actionSelector);
    connect(actionSequence, SIGNAL(triggered()), this, SLOT(insertChild()));
    compositeGroup.addAction(actionSequence);
    connect(actionSelectorParallel, SIGNAL(triggered()), this, SLOT(insertChild()));
    compositeGroup.addAction(actionSelectorParallel);
    connect(actionSequenceParallel, SIGNAL(triggered()), this, SLOT(insertChild()));
    compositeGroup.addAction(actionSequenceParallel);
    connect(actionSelectorND, SIGNAL(triggered()), this, SLOT(insertChild()));
    compositeGroup.addAction(actionSelectorND);
    connect(actionSequenceND, SIGNAL(triggered()), this, SLOT(insertChild()));
    compositeGroup.addAction(actionSequenceND);
    connect(actionRemove_Behavior, SIGNAL(triggered()), this, SLOT(removeBehavior()));
    
    // Decisions Menu
    connect(menuDecisions, SIGNAL(aboutToShow()), this, SLOT(updateDecisions()));
    connect(actionRemove_Decision, SIGNAL(triggered()), this, SLOT(removeDecision()));
    //connect(actionInsert_Node, SIGNAL(triggered()), this, SLOT(insertChild()));
    //nodeGroup.addAction(actionNode);

    updateBehaviors();
    updateDecisions();
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::btSelectionCallback(const QItemSelection& selected, const QItemSelection& /*deselected*/)
{
    if (selected.indexes().isEmpty()) return;

    TreeModel *dtModel = dynamic_cast<TreeModel*>(dtTreeView->model());
    if (!dtModel) return;

    QModelIndex selIdx = selected.indexes().at(0);
    TreeItem* selItem = static_cast<TreeItem*>(selIdx.internalPointer());
    if (!selItem) return;
    
    dtModel->clear();
    
    QMap<QString, QVariant> data;
    data["Name"] = selItem->iName().toString();
    data["iName"] = selItem->name().toString();
    data["fName"] = compositeClasses.key(qMakePair(data["iName"].toString(), data["Name"].toString()));
    // TODO: these values will change when we implement a DT
    data["ID"] = "interrupt";
    data["parentID"] = "none";
    
    dtModel->addItem(dtModel->createItem(data));
}

void MainWindow::dtSelectionCallback(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/)
{
}

void MainWindow::idChangedCallback(const QModelIndex& topLeft, const QModelIndex& /*botRght*/)
{
    // we only care about ID's (for now), and assume we only have one object selected
    if (!topLeft.isValid() || topLeft.column() != 1)
        return;

    TreeItem *item = static_cast<TreeItem*>(topLeft.internalPointer());
    if (item)
        item->id(topLeft.data().toString());
}

void MainWindow::updateBehaviors()
{
    // Process the actions file for actions. Don't worry about overhead since the file is
    // never going to get too big. This way we don't have to think when we update the file
    // since it will always reprocess when opening the menu.
    QList<QAction*> actions = menuInsert_Action->actions();
    for (QList<QAction*>::iterator it = actions.begin(); it != actions.end(); ++it)
    {
        menuInsert_Action->removeAction(*it);
    }

    QFile actionsFile(scriptsDir.absoluteFilePath("actions/actions.lua"));
    readDefs(actionsFile);
    
    QFile compositeFile(scriptsDir.absoluteFilePath("tasks/tasks.lua"));
    readDefs(compositeFile);
}

void MainWindow::readDefs(QFile& actionsFile)
{
    if (!actionsFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&actionsFile);
    while (!in.atEnd())
    {
        QStringList line = in.readLine().split('"');
        if (line.length() < 2)
            continue;

        QFile actionFile(scriptsDir.absoluteFilePath(line.at(1)));
        QRegExp classDefs("(.*) = createClass\\((.*),\\s*(.*)\\)");

        if (!actionFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QTextStream fullIn(&actionFile);
        while (!fullIn.atEnd())
        {
            QString contents = fullIn.readLine();

            if (!classDefs.exactMatch(contents)) continue;
            
            QString actionText = classDefs.capturedTexts().at(1);
            compositeClasses[actionText] = qMakePair(classDefs.capturedTexts().at(2),
                                                     classDefs.capturedTexts().at(3));
           
            if (actionText.startsWith("Composite")) continue;
            
            QAction *newAction = new QAction(actionText, this);
            menuInsert_Action->addAction(newAction);

            if (QStringRef(&actionText, 0, 2) == "is")
                checkGroup.addAction(newAction);
            else //if (QStringRef(&actionText, 0, 2) == "anything else")
                actionGroup.addAction(newAction);
            
            connect(newAction, SIGNAL(triggered()), this, SLOT(insertChild()));
        }
    }
}

void MainWindow::updateDecisions()
{
    QList<QAction*> decisions = menuInsert_Leaf->actions();
    for (QList<QAction*>::iterator it = decisions.begin(); it != decisions.end(); ++it)
        menuInsert_Leaf->removeAction(*it);

    {
        QAction *newDecision = new QAction("Interrupt", this);
        menuInsert_Leaf->addAction(newDecision);

        leafGroup.addAction(newDecision);

        connect(newDecision, SIGNAL(triggered()), this, SLOT(insertChild()));
    }
    
    QFile decisionsFile(scriptsDir.absoluteFilePath("interrupts.lua"));
    
    if (!decisionsFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    
    QTextStream in(&decisionsFile);
    QRegExp classDefs("(.*) = createClass.*");
    while (!in.atEnd())
    {
        QString line = in.readLine();
        
        if (classDefs.exactMatch(line))
        {
            QString decisionText = classDefs.capturedTexts().at(1);
            QAction *newDecision = new QAction(decisionText, this);
            menuInsert_Leaf->addAction(newDecision);
            
            leafGroup.addAction(newDecision);
            
            connect(newDecision, SIGNAL(triggered()), this, SLOT(insertChild()));
        }
    }
}

void MainWindow::openFileDialog()
{
    QString fileSelection = QFileDialog::getOpenFileName(this, tr("Open Template File"), currentFileInfo.absolutePath(), tr("Lua (*.lua)"));
    if (fileSelection.isEmpty())
        return;

    currentFileInfo = fileSelection;
    if (!currentFileInfo.isFile()) return;

    currentFile.close();
    currentFile.setFileName(currentFileInfo.absoluteFilePath());

    if (!currentFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QMap<QString, QStringList> actions;

    QTextStream tempIn(&currentFile);
    while (!tempIn.atEnd())
    {
        QStringList line = tempIn.readLine().split(',');
        if (line.length() < 4) continue;

        for (QStringList::iterator it = line.begin(); it != line.end(); ++it)
            it->remove('{').remove('}').remove('"').remove('\t').remove(' ');

        actions[line.at(0)] = line;
    }
    
    currentFile.close();
    
    // This ensures that we don't have NULL models, so no need to check
    clear();

    TreeModel *btModel = dynamic_cast<TreeModel*>(btTreeView->model());
    //TreeModel *dtModel = dynamic_cast<TreeModel*>(dtTreeView->model());
    
    // for now assume we opened a template and parse it
    // each action has 4 entries (after trimming). The first is the id, and
    // is what the map is keyed to. The second is the name of the action lua
    // class. The third is the parent id. The fourth is the type of action
    // (selector, sequence, or behavior)
    
    // To ensure that we always have the id's to associate with parent id's,
    // build a list of everything first and then go back and assign the parents
    // and set up the display
    QMap<QString, QList<QMap<QString, QVariant> > > parents;
    for (QMap<QString, QStringList>::iterator it = actions.begin(); it != actions.end(); ++it)
    {
        QMap<QString, QVariant> data;
        
        QString refName = it->at(1);
        
        if (it->at(3) == "SELECTORBEHAVIOR")
            data["Name"] = "Selector";
        else if (it->at(3) == "SEQUENCEBEHAVIOR")
            data["Name"] = "Sequence";
        else if (it->at(3) == "PARALLELSELECTORBEHAVIOR")
            data["Name"] = "SelectorParallel";
        else if (it->at(3) == "PARALLELSEQUENCEBEHAVIOR")
            data["Name"] = "SequenceParallel";
        else if (it->at(3) == "NONDETERMINISTICSELECTORBEHAVIOR")
            data["Name"] = "SelectorNonDeterministic";
        else if (it->at(3) == "NONDETERMINISTICSEQUENCEBEHAVIOR")
            data["Name"] = "SequenceNonDeterministic";
        else
            data["Name"] = compositeClasses[refName].first;
        
        data["iName"] = compositeClasses[refName].second;
        data["fName"] = refName;
        
        data["ID"] = it->at(0);
        data["parentID"] = it->at(2);
        
        parents[data["parentID"].toString()].append(data);
    }
    
    // start with the parentID of "none" since this is necessarily the root
    // of the tree
    // parentID, parentItem
    QMap<QString, TreeItem*> idsToParse;
    {
        QList<QMap<QString, QVariant> > rootItems = parents.take("none");
        // just take the first entry, it should only have one entry so we don't
        // have any way to deal with the others anyway.
        QMap<QString, QVariant> rootData = rootItems.takeFirst();
        // no parent
        TreeItem *result = btModel->createItem(rootData);
        btModel->addItem(result);
        idsToParse[rootData["ID"].toString()] = result;
    }
    
    // Now loop through the rest and add them where they need to be
    while (!idsToParse.isEmpty())
    {
        QString currentID = idsToParse.keys().at(0);
        Node *currentParent = dynamic_cast<Node*>(idsToParse[currentID]);
        idsToParse.remove(currentID);
        if (!currentParent) continue;
        
        QList<QMap<QString, QVariant> > currentItems = parents.take(currentID);
        
        for (QList<QMap<QString, QVariant> >::iterator it = currentItems.begin(); it != currentItems.end(); ++it)
        {
            TreeItem *result = btModel->createItem(*it, currentParent);
            btModel->addItem(result);
            idsToParse[(*it)["ID"].toString()] = result;
        }
    }

    // now we have a map of all items keyed by ID with parents assigned properly
    // TODO: Feature add: edit lua actions in the editor
}

void MainWindow::openDirDialog()
{
    QString dirSelection = QFileDialog::getExistingDirectory(this, tr("Set Script Directory"), scriptsDir.absolutePath());
    if (dirSelection.isEmpty())
        return;

    scriptsDir = dirSelection;

    // TODO: process the directory to populate the action menu
    //  possibly a popout tool selector like dia
    //  or a scrollbox in the first column (is that even doable?)
    //  or a menu subitem to insert action and insert composite
}

void MainWindow::openSaveDialog()
{
    QString filter = "Lua (*.lua)";
    QString saveFile = QFileDialog::getSaveFileName(this, tr("Save File"), currentFileInfo.absolutePath(), filter, &filter);
    if (saveFile.isEmpty()) return;
    if (!saveFile.endsWith(".lua")) saveFile += ".lua";

    // first write to templates.lua
    QStringList templateList = readFileIntoList(scriptsDir.absoluteFilePath("templates/templates.lua"));
    
    // find out if we need to change a line and if we do, write the list back out
    QString base = QFileInfo(saveFile).baseName();
    QString templateEntry = "includeAiFile(\"templates/" + base + ".lua\"";
    if (!templateList.contains(templateEntry))
    {
        // if it isn't found, it will give idx = 0, which will simply prepend
        int idx = templateList.lastIndexOf(QRegExp("includeAiFile.*")) + 1;
        if (idx > templateList.size()) idx = templateList.size();
        templateList.insert(idx, templateEntry);
        
        writeListIntoFile(templateList, scriptsDir.absoluteFilePath("templates/templates.lua"));
    }
    
    QStringList actionList = readFileIntoList(scriptsDir.absoluteFilePath("actions/actions.lua"));
    QStringList taskList = readFileIntoList(scriptsDir.absoluteFilePath("tasks/tasks.lua"));
    for (QMap<QString, QPair<QString, QString> >::const_iterator it = compositeClasses.begin();
         it != compositeClasses.end(); ++it)
    {
        QStringList *currentList = &actionList;
        if (it->first.startsWith("Composite"))
            currentList = &taskList;
        
        QString line = it.key() + " = createClass(" + it->first + ", " + it->second + ")";
        if (!currentList->contains(line))
        {
            int idx = currentList->lastIndexOf(QRegExp(".*createClass.*,.*")) + 1;
            if (idx == 0)
            {
                idx = currentList->lastIndexOf(QRegExp("includeAiFile.*")) + 1;
                line.prepend("\n");
            }
            
            if (idx > currentList->size()) idx = currentList->size();
            currentList->insert(idx, line);
        }
    }
    writeListIntoFile(actionList, scriptsDir.absoluteFilePath("actions/actions.lua"));
    writeListIntoFile(taskList, scriptsDir.absoluteFilePath("tasks/tasks.lua"));

    // now write out the actual template. Don't worry about checking if it already exists,
    // we can assume that since we are saving that it should be written.
    {
        QFile file(saveFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

        QTextStream stream(&file);

        TreeModel* btModel = dynamic_cast<TreeModel*>(btTreeView->model());
        if (!btModel) return;

        stream << base << " = {\n";
        btModel->write(stream);
        stream << "}\n\naddAiTemplate(\"" << base << "\", " << base << ")";
    }
}

QStringList MainWindow::readFileIntoList(const QString& fileName)
{
    QStringList returnList;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return returnList;
    
    QTextStream stream(&file);
    
    while (!stream.atEnd())
        returnList << stream.readLine();
    
    return returnList;
}

void MainWindow::writeListIntoFile(const QStringList& list, const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    
    QTextStream stream(&file);
    
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        stream << *it << "\n";
}

void MainWindow::clear()
{
    TreeModel* model = dynamic_cast<TreeModel*>(btTreeView->model());
    if (model == NULL) btTreeView->setModel(new TreeModel());
    else model->clear();

    model = dynamic_cast<TreeModel*>(dtTreeView->model());
    if (model == NULL) dtTreeView->setModel(new TreeModel());
    else model->clear();
}

void MainWindow::loadSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    
    QString pathName = settings.value("general/scriptsDir", tr("/home/swgemu/MMOCoreORB/bin/scripts/ai")).toString();
    scriptsDir = QDir(pathName, tr("Lua (*.lua)"));
    if (!scriptsDir.exists())
    {
        scriptsDir = QDir::currentPath();
        scriptsDir.setNameFilters(QStringList("Lua (*.lua)"));
    }

}

void MainWindow::saveSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("general/scriptsDir", scriptsDir.path());
}

void MainWindow::insertChild()
{
    QAction *senderAction = dynamic_cast<QAction*>(sender());
    if (senderAction == NULL) return;
	
    TypeGroup *senderGroup = dynamic_cast<TypeGroup *>(senderAction->actionGroup());
    if (!senderGroup) return;

    QModelIndex index = btTreeView->selectionModel()->currentIndex();

    TreeModel* model = NULL;
    TreeItem* newItem = NULL;
    if (senderGroup->isBehavior())
    {
        model = dynamic_cast<TreeModel*>(btTreeView->model());
        if (model) newItem = model->addItem(senderGroup, index);
    }
    else if (senderGroup->isDecision())
    {
        if (!index.isValid()) return; // don't add DT's without BT item references
        model = dynamic_cast<TreeModel*>(dtTreeView->model());
        if (model) newItem = model->addItem(senderGroup, dtTreeView->selectionModel()->currentIndex());
    }

    if (!model || !newItem) return;
    
    if (senderAction->text().startsWith("Sequence") || senderAction->text().startsWith("Selector"))
    {
        newItem->name(senderAction->text());
        newItem->iName(QString("Interrupt"));
        newItem->fName(QString("Composite"));
    }
    else
    {
        QPair<QString, QString> names = compositeClasses[senderAction->text()];
        newItem->name(names.first);
        newItem->iName(names.second);
        newItem->fName(senderAction->text());
    }

    if (senderGroup->isDecision())
    {
        TreeItem *bt = static_cast<TreeItem*>(index.internalPointer());
        if (bt)
        {
            newItem->iName(bt->name().toString());
            newItem->name(newItem->fName().toString());
            newItem->id(QString("interrupt")); // TODO this will obviously change
            bt->iName(newItem->name().toString());
            QPair<QString, QString> compPair = qMakePair(bt->name().toString(), bt->iName().toString());
            if (compositeClasses.key(compPair).isEmpty())
            {
                QString left = compPair.first;
                if (left.endsWith("Base")) left.chop(4);
                
                QString right = compPair.second;
                if (right.endsWith("Interrupt")) right.chop(9);
                
                compositeClasses[left + right] = compPair;
            }
        }
    }

    btTreeView->resizeColumnToContents(0);
    btTreeView->resizeColumnToContents(1);
}

void MainWindow::insertChildBehavior(TreeItem* behavior)
{
    TreeModel *btModel = dynamic_cast<TreeModel*>(btTreeView->model());
    if (!btModel) return;
    
    if (!btModel->addItem(behavior)) return;
    
    btTreeView->resizeColumnToContents(0);
    btTreeView->resizeColumnToContents(1);
}

void MainWindow::insertChildDecision(TreeItem* decision)
{
    TreeModel *dtModel = dynamic_cast<TreeModel*>(dtTreeView->model());
    if (!dtModel) return;
    
    if (!dtModel->addItem(decision)) return;
    
    dtTreeView->resizeColumnToContents(0);
    dtTreeView->resizeColumnToContents(1);
}

void MainWindow::removeBehavior()
{
    TreeModel *btModel = dynamic_cast<TreeModel*>(btTreeView->model());
    if (!btModel) return;
    
    QModelIndex idx = btTreeView->selectionModel()->currentIndex();
    
    btModel->removeItem(idx);
}

void MainWindow::removeDecision()
{
    TreeModel *dtModel = dynamic_cast<TreeModel*>(dtTreeView->model());
    if (!dtModel) return;
    
    QModelIndex idx = dtTreeView->selectionModel()->currentIndex();
    
    dtModel->removeItem(idx);

    QModelIndex index = btTreeView->selectionModel()->currentIndex();
    if (index.isValid() && static_cast<TreeItem*>(index.internalPointer()))
        static_cast<TreeItem*>(index.internalPointer())->iName(QString(""));
}
