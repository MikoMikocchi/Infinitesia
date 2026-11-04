Я бы строил это как Dwarf Fortress RAW, доведённый до уровня универсальной научно-технологической симуляции цивилизации и планетарной системы.

Причём я бы не стал просто добавлять в Dwarf Fortress новые категории. Я бы сохранил его главный архитектурный принцип:

- Есть множество типов определений объектов.
- Каждый объект собирается из большого количества параметров и токенов.
- Объекты могут ссылаться друг на друга.
- Одни определения становятся основой для других.
- Игровой мир состоит из экземпляров этих определений.
- Игровые системы взаимодействуют между собой через общие данные.

Но добавил бы три фундаментальные вещи:

1. Универсальную систему физических и химических свойств — чтобы материал существовал не просто как название, а как полноценный физический объект.
2. Универсальную систему процессов — чтобы химические реакции, металлургия, геологические процессы, работа механизмов и биологические процессы описывались совместимым способом.
3. Универсальную систему технологического развития — чтобы цивилизации могли открывать технологии, осваивать научные дисциплины, проходить технологические эпохи и развивать собственную промышленность.

Самое главное: я бы сделал так, чтобы ты мог создать практически что угодно из базовых определений, не программируя каждый конкретный объект отдельно.

Например, новый минерал, планету, организм, государство, промышленную машину или даже целую цивилизацию.

# 1. Общая архитектура

Я бы разделил весь data-driven слой на 15 больших доменов.

# UNIVERSE DATA

Общая база определений вселенной

01

Cosmology

Космос и астрономия

02

Planetology

Формирование планет

03

Geoscience

Геология и география

04

Materials

Вещества и материалы

05

Physics

Физика и энергия

06

Chemistry

Химические процессы

07

Resources

Ресурсы и добыча

08

Industry

Машины и производство

09

Biology

Жизнь и организмы

10

Ecology

Экосистемы

11

Technology

Наука и исследования

12

Society

Общества и культуры

13

Politics

Государства и политика

14

Economy

Экономика и торговля

15

Infrastructure

Инфраструктура и транспорт

UNIVERSE SIMULATION

Конкретные планеты, материалы, существа, государства, машины, цивилизации и исторические события

Это именно логические домены. Внутри каждого будут собственные RAW-типы, а некоторые типы будут использоваться сразу несколькими доменами.

# 2. Полный каталог типов данных

Ниже я бы уже проектировал конкретные типы определений. По стилю это будет близко к `CREATURE`, `INORGANIC`, `ITEM`, `REACTION`, `ENTITY` из Dwarf Fortress.

Обозначения:

- RAW Type — самостоятельный тип определения, примерно как в DF.
- Примеры — какие именно сущности можно будет создавать внутри него.

## I. Cosmology — космология и астрономия

[Scientists find a solar system that makes no sense: Discover evidence of ‘inside-out’ planet formation - The Economic Times](https://images.openai.com/static-rsc-4/-DAOzzRUbkz0LbM6ek093NMFQitct67cAgs84dk_SlGEvi1yOgRV_fGJBr6O2pgr54sZX_3dGs1BRbCuOzgvdxYTfd5-DLj_trdRUUJKbrR5VuyhPlUFruiwzyjf60JHLL6vCOxqMgVn0BCPAbHjSnrgDxJiAi0iwSn8QaebWSA?purpose=inline)

[Astronomers can now measure newborn planet mass from dusty stellar rings - The Brighter Side of News](https://images.openai.com/static-rsc-4/4p7bgGjB4J1NUkW-01TSay0uhVJyaIhF4QwZWwDuGS2B37o0PaH5FDpOroHZcnjtbsSCoIYbFcVTlWeMWTPhsnVuvRg-rNTi-xCU7dPAHdUvHIhFJjqi7pJW0B2jZGf81d_TgHA77UEiyXs6bpIBgQJk1Gik2yu6cXH3FLNz34A?purpose=inline)

[Spiral galaxy in deep space | Free Photo  - rawpixel](https://images.openai.com/static-rsc-4/x1YK7O-Mh5yhrwDTYnof5zq9hDqf4Rv_OdWSpavxund4FidAOKWtNcYIbtnXKgCFzlqngymuYGRVvGAthRaFnvGbPzWOdLBNxfPlqNXH8CoJ8LdMOs046uawZpsrVvV8tQOkxwD99CcETCmGNA5XWcBuf-fKhu8TBlGb_6l1lLk?purpose=inline)

7

| RAW Type                | Содержание                 | Примеры                                       |
| ----------------------- | -------------------------- | --------------------------------------------- |
| `STAR`                  | Звёзды                     | Красный карлик, жёлтый карлик, голубой гигант |
| `STAR_CLASS`            | Классификации звёзд        | G, K, M, O                                    |
| `STELLAR_SYSTEM`        | Звёздные системы           | Система из звезды и планет                    |
| `CELESTIAL_BODY`        | Небесные тела              | Планеты, луны, астероиды                      |
| `PLANET_TYPE`           | Типы планет                | Газовый гигант, земная, ледяная               |
| `ORBIT`                 | Орбитальные характеристики | Орбиты планет и спутников                     |
| `SATELLITE`             | Спутники                   | Естественные и искусственные                  |
| `ASTEROID`              | Астероидные объекты        | Астероиды, карликовые планеты                 |
| `COMET`                 | Кометы                     | Ледяные тела                                  |
| `COSMIC_EVENT`          | Космические события        | Вспышки звёзд, столкновения                   |
| `ASTROPHYSICAL_PROCESS` | Астрофизические процессы   | Аккреция, звёздная эволюция                   |
| `SPACE_REGION`          | Пространственные области   | Межзвёздное пространство, туманности          |

## II. Planetology — формирование и устройство планет

[Origin of life-forming volatile elements in the inner Solar System | Nature](https://images.openai.com/static-rsc-4/MgJsfgw1ystnfxwYVDRJ_O7Rn0BeRKeQL6GqvoIb-aPUDkPDw5SFLyx_rDwJTx-oYgBcYAHFmBB3yZS-OhHypblRSo29Jim93rMnhM_YOwr685N6ns9zbF3NKkxEnGwiGWfQ2qouCaEIFvrQKZ2aDEpB3fJxsZvzN1D2kVSnWzQ?purpose=inline)

[Astronomy 104: Lecture 21 printable version](https://images.openai.com/static-rsc-4/-MW23BeO4SjWJco2OiauT_BzMfMyhIqPERu0vTzKvU-7QlF9HthyXq-CKOUwGKKMUkPD57E3QeO5c0u0JUraT4BkYHlD9wYIS-uBckPzIPCkonLs70ENZGlLMOAfmGGaSjiQiTSsZE8OoIHynDAyAO43wqf2gPy1KAsIMPaqwj0?purpose=inline)

[Module 1 Origin and Structure of the Earth Planet Earth.pptx](https://images.openai.com/static-rsc-4/jn8cVyb9jW5hBJylTKcDb8WmsuIhibPy4SYFo5Phn9Yf-D6wBn9pIkXApQUD19kHnPjMl07a0FTwl7Oj0TNxO_DWJAFneDDO004gF6eRaXIpl9mWSpdDTCj1M_9zGiMrL22k085SRUNL8dcCLKm0L0ZppgNldObtPHWixqR7fjk?purpose=inline)

5

| RAW Type                | Содержание            | Примеры                         |
| ----------------------- | --------------------- | ------------------------------- |
| `PLANET_TEMPLATE`       | Шаблоны планет        | Каменистая, океаническая        |
| `PLANET_LAYER`          | Внутренние слои       | Ядро, мантия, кора              |
| `PLANET_FORMATION`      | Сценарии формирования | Аккреция, дифференциация        |
| `PLANETARY_COMPOSITION` | Состав планет         | Элементный и минеральный состав |
| `TECTONIC_PLATE`        | Тектонические плиты   | Континентальные, океанические   |
| `TECTONIC_BOUNDARY`     | Границы плит          | Конвергентные, дивергентные     |
| `GEOLOGICAL_EVENT`      | Геологические события | Землетрясения, извержения       |
| `PLANETARY_FIELD`       | Планетарные поля      | Магнитное, гравитационное       |
| `ATMOSPHERE`            | Атмосферы             | Состав и структура              |
| `HYDROSPHERE`           | Гидросфера            | Океаны, моря, подземные воды    |
| `PLANETARY_CYCLE`       | Планетарные циклы     | Углеродный, водный              |
| `PLANETARY_PROPERTY`    | Параметры планет      | Масса, плотность, радиус        |

## III. Geoscience — география, климат, гидрология и геология

[加拿大BC省的魅力景色都在这些小清新里！一文看遍好风景 - 加拿大乐活网](https://images.openai.com/static-rsc-4/2p-GUZOCLC1PN20CSgQKa5gdIk_BmAHnI2AxrNIpikAl3dG9P_Gg6eYVWAJzNbOrtUhmwZXYwZt28MZ4CcRLLLel84zY4FURwkKDeGeSD78kAljZU5rOv1XnUHVRsIl99vKW7cDDMXq19xa_JbVMWlrIG1JyiRmuUp2MlV68Nxk?purpose=inline)

[Scientific Illustration AI Images | Morphic](https://images.openai.com/static-rsc-4/1j2jqICSJWqJSOSGZpDWErd-pBtEpn-bACdaDy2EK9vIAZxZ2QJaGQd1nedtiremNrrMnQrlLCX6uhmjIiV5vmhVIRN3znvVpW9eFTnNlqBWCWVEaP2whoAFMaY40GSC2RrU_MQvu2uLxTOBWRwjpeo5ylFv3unxmKqN2k_eg4k?purpose=inline)

[File:Biomes of the World - Retouched.png - Wikimedia Commons](https://images.openai.com/static-rsc-4/uUdy4f180qdIqXrr54djRKnpt58H86r03IsPyHDqdipz6t8X6PSixmu1weDXWnQSV0fFoV9yHUYAZDnhGFVtomgHQaOABfepJpd_HxVBtxHzhuqoGSADE41O2ovCOS8xnIFZ7LF9h9xhWVNMhv2qANEgl6zZiapzdny5b5Mwcz8?purpose=inline)

7

| RAW Type               | Содержание               | Примеры                            |
| ---------------------- | ------------------------ | ---------------------------------- |
| `BIOME`                | Биомы                    | Тайга, пустыня, тундра             |
| `TERRAIN`              | Типы местности           | Равнина, горы, плато               |
| `GEOLOGICAL_LAYER`     | Геологические слои       | Осадочные, магматические           |
| `ROCK_TYPE`            | Горные породы            | Гранит, базальт, известняк         |
| `MINERAL`              | Минералы                 | Кварц, кальцит, оливин             |
| `ORE`                  | Рудные образования       | Железные, медные руды              |
| `DEPOSIT`              | Месторождения            | Жилы, пласты, залежи               |
| `SOIL_TYPE`            | Почвы                    | Чернозём, глина, песок             |
| `CLIMATE`              | Климатические режимы     | Тропический, арктический           |
| `WEATHER_PATTERN`      | Погодные явления         | Циклоны, фронты                    |
| `WATER_BODY`           | Водные объекты           | Озёра, реки, океаны                |
| `WATER_TYPE`           | Типы воды                | Пресная, солёная, минерализованная |
| `HYDROLOGICAL_PROCESS` | Гидрологические процессы | Эрозия, испарение, инфильтрация    |
| `EROSION_TYPE`         | Типы эрозии              | Ветровая, водная, ледниковая       |
| `VOLCANIC_SYSTEM`      | Вулканические системы    | Вулканы, магматические камеры      |
| `GEOMORPHOLOGY`        | Формы рельефа            | Долины, каньоны, дельты            |

## IV. Materials — материалы и вещества

[Metallische Rohstoffe, Sammlung | www.der-hedinger.de](https://images.openai.com/static-rsc-4/9CznfJcn8dzbPar2czAdsvGnQ88rw4hVw0QoLD4HOi_2xm2TljxsQNkNUVBzepWS8gX3fZUj_A6d_E_Cz6Cy5BYNy9yIT-_A3to9OYL3egvdn-8vYOVnXsLD9MBKr0jz8Y8mnUi8R5F9HT5sqtScvIZWVs-KGUaF6SAy8tns034?purpose=inline)

[7637 白銅 進化し続ける｢モノづくりの応援団｣ | 有力企業 | 東洋経済オンライン](https://images.openai.com/static-rsc-4/tnSPEyARs4EgJ1i31DGc9DeHC9-2gwEGYIt9v-y6K6Wt56Z8NzTJBrO-qAVsPfFNyStJI4u3BV6Eu3K1f41ryeghDVUQD_stF8J0ACznELMYaqueIYAFhbm0CqJPMVEXQTC5TrHfwkT2aMCnSiD0Pqs721naAyQsRWLgGkKXuSo?purpose=inline)

[Overview exhibitions - Deutsches Museum](https://images.openai.com/static-rsc-4/Ths9xjaExCvP_F-LfvyhKMmWK7PVpB4AnbW8AOhJu57G44vl6t_AVzYc_xLcDtefwozQklnJFA-xaznNdBZanclMZO2TEGDQYGoXy6JrM3QMrTHurj4b2gtTeosszxmDzjwM0ZD15BUEDNFXv4Lrt9MbDdlNGXccqSLH5OpbaGw?purpose=inline)

8

Это один из самых важных доменов. Я бы сделал его центральным для всей игры.

| RAW Type                  | Содержание               | Примеры                             |
| ------------------------- | ------------------------ | ----------------------------------- |
| `ELEMENT`                 | Химические элементы      | Fe, Cu, Au, C, Si                   |
| `ISOTOPE`                 | Изотопы                  | Fe-56, C-14                         |
| `COMPOUND`                | Химические соединения    | H₂O, SiO₂, Fe₂O₃                    |
| `MATERIAL`                | Общие материалы          | Сталь, стекло, бетон                |
| `MATERIAL_TEMPLATE`       | Шаблоны материалов       | Металл, керамика, полимер           |
| `MATERIAL_CLASS`          | Классы материалов        | Металлы, диэлектрики                |
| `MATERIAL_PROPERTY`       | Свойства материалов      | Плотность, твёрдость, теплоёмкость  |
| `MATERIAL_PHASE`          | Фазовые состояния        | Твёрдое, жидкое, газообразное       |
| `ALLOY`                   | Сплавы                   | Бронза, латунь, сталь               |
| `MINERAL_COMPOSITION`     | Минеральные составы      | Состав руды и породы                |
| `FLUID`                   | Жидкие вещества          | Вода, кислота, расплав              |
| `GAS`                     | Газообразные вещества    | Кислород, азот, CO₂                 |
| `POWDER`                  | Порошкообразные вещества | Порошки металлов, оксиды            |
| `FIBER`                   | Волокнистые материалы    | Натуральные и синтетические волокна |
| `TISSUE_TEMPLATE`         | Биологические ткани      | Мышечная, костная, нервная          |
| `MATERIAL_TRANSFORMATION` | Изменения материалов     | Плавление, спекание, полимеризация  |

Особенно важно разделить `ELEMENT`, `COMPOUND` и `MATERIAL`.

Например:

- Железо — `ELEMENT`.
- Оксид железа — `COMPOUND`.
- Сталь — `ALLOY` и одновременно `MATERIAL`.
- Железная руда — геологический объект, содержащий минеральные компоненты.
- Стальной блок — форма представления конкретного материала.

Таким образом, химическая сущность не должна зависеть от того, в каком виде игрок её использует.

## V. Physics & Energy — физика и энергетика

| RAW Type                | Содержание          | Примеры                                  |
| ----------------------- | ------------------- | ---------------------------------------- |
| `PHYSICAL_PROPERTY`     | Физические свойства | Масса, давление, температура             |
| `PHYSICAL_FIELD`        | Физические поля     | Электрическое, магнитное, гравитационное |
| `ENERGY_TYPE`           | Типы энергии        | Кинетическая, тепловая, электрическая    |
| `ENERGY_SOURCE`         | Источники энергии   | Солнце, уголь, реактор                   |
| `ENERGY_STORAGE`        | Накопители          | Батареи, аккумуляторы, маховики          |
| `ENERGY_CONVERTER`      | Преобразователи     | Генераторы, двигатели                    |
| `ENERGY_TRANSFER`       | Передача энергии    | Теплопроводность, электрическая передача |
| `FORCE_TYPE`            | Типы сил            | Трение, упругость, гравитация            |
| `THERMODYNAMIC_PROCESS` | Термодинамика       | Нагрев, охлаждение, фазовый переход      |
| `RADIATION_TYPE`        | Излучение           | Электромагнитное, ионизирующее           |
| `PHYSICS_MODEL`         | Физические модели   | Механика, термодинамика, электродинамика |

## VI. Chemistry — химия

[ChemSetu | API Impurities & Custom Synthesis Solutions](https://images.openai.com/static-rsc-4/PQ30vAocsDGVan-8erJPe0JrLTLwBJCdkjlMlVslOcsNRkk4aCbtkTSp3Oo04o8IFhK5piMJr5RhV3alhUjTxjI6fowsAGwFQ02YlMYiUZl5zum4i5f3aILOcU07-TsB98OXFmTb9WfZjvWelbgG-406q0aE7jiHee48FrlqXRw?purpose=inline)

[Brenntag acquires Singapore-based industrial chemicals distributor Aik Moh Group](https://images.openai.com/static-rsc-4/O9rehnN33pKUPjF3IAZy4YgkdwERJJ10zvJuKfGycdmGI6ue7bWnxm4MJc3m8MEu6cuj1Q-Ayspn6fAMPkjMKTEx8lZ_shWk33wnmE0UVbRUccLEggi-KHir12iC9xGsEgX5GQQSlJaW-HUK3rxLUl5a3pMreVlwNFq1-FEuIbw?purpose=inline)

[ניטור בזקנה ובמחלות ניווניות באמצעות בינה מלאכותית ושעונים חכמים | הפקולטה למדעי הרפואה והבריאות | אוניברסיטת תל אביב](https://images.openai.com/static-rsc-4/9ulfd4p_7BlG5mMOX4f4xy6UQeCQE4cR8U2yBaW5g4gAKFSEaOYcDVGGHKp4JRT1gWqaKbmozoyhCzLmlr5CBURJGG_x9_mVhDMGywkt5Tvkk--8pbtuvTlC4_kgNVEwXB6aX5aGriEcXgpwE9ZxmdIHy0Y3FXjCkRIyCofUcDU?purpose=inline)

7

| RAW Type             | Содержание            | Примеры                        |
| -------------------- | --------------------- | ------------------------------ |
| `CHEMICAL_REACTION`  | Химические реакции    | Окисление, восстановление      |
| `REACTION_TEMPLATE`  | Шаблоны реакций       | Общие типы реакций             |
| `REACTION_CATALYST`  | Катализаторы          | Каталитические вещества        |
| `REACTION_CONDITION` | Условия реакции       | Температура, давление          |
| `REACTION_BYPRODUCT` | Побочные продукты     | Шлаки, газы, осадки            |
| `CHEMICAL_PROCESS`   | Химические процессы   | Электролиз, дистилляция        |
| `SOLUTION`           | Растворы              | Водные растворы                |
| `MIXTURE`            | Смеси                 | Минеральные и химические смеси |
| `ACID_BASE`          | Кислоты и основания   | Кислоты, щёлочи                |
| `CHEMICAL_PROPERTY`  | Химические свойства   | Реакционная способность        |
| `CHEMICAL_BOND`      | Типы связей           | Ионная, ковалентная            |
| `CHEMICAL_GROUP`     | Функциональные группы | Гидроксильные, карбоксильные   |
| `SEPARATION_PROCESS` | Разделение веществ    | Фильтрация, центрифугирование  |

Я бы предусмотрел поддержку стехиометрии, ограничивающих реагентов, выхода продукта, кинетики и термодинамических условий.

Тогда одна система реакций сможет обслуживать:

- Химическую лабораторию.
- Металлургический завод.
- Геологические процессы.
- Биохимические процессы.
- Производство полимеров.
- Переработку руд.
- Работу промышленных реакторов.

## VII. Resources & Extraction — ресурсы и добыча

[Core Lithium awards $274m underground mining contract to Develop Global for BP33 deposit in Northern Territory](https://images.openai.com/static-rsc-4/NDXmi9Q-X6TgXLgRw1gyJda2Oc7yMwg-eHz-PAFiExmhrgqQ59kwhxEizhjQF5eDfqALUaQymmdF3hPjVlaSyAmO1IAYoIinMIf8aw_4lXgh614pQr-3af1XPOJMsEoErDzr25DrXVPVlvJd0XQnHehbLLE-4Juwh848fjkxENM?purpose=inline)

[Kazakhstan aims to boost copper output to 750,000 tons by 2029&nbsp;](https://images.openai.com/static-rsc-4/XzZJT9EQkWYrpdmDqLw5KsRNdpsZben8ycXP62MFW-BMQTUaOINjxdDwSW-8Nfx9Zd2qwaV9kRVnUyHjaWN2v6AjKjyEbXLiccIFEOuDnHuO2TXlSZWAGT4sTnnNycb-YrkU0pu0X95sPITh5zQ39kcp3grcA2XNNxJ6HJxfHQk?purpose=inline)

[Underground Mining Ecosystem: Engineering Precision Beneath the Surface | Auqib Javed posted on the topic | LinkedIn](https://images.openai.com/static-rsc-4/K9Pd1Qt3dmuqO895wCQzTcqNPenc_toORhmCwlg1M-TIjQVo4aE6NT0rFzFkEl7TxfKRrVa-P2-VQhP4cw00V-QwOkDzp5823QCkFyGUckS6Qp63jY8Ml9PbZ3UCe83m1uNoC8nXkItUulxbqymfMyWD1BFDAixb2hOT_Eixju8?purpose=inline)

10

| RAW Type            | Содержание      | Примеры                       |
| ------------------- | --------------- | ----------------------------- |
| `RESOURCE`          | Общие ресурсы   | Руды, нефть, вода             |
| `RESOURCE_DEPOSIT`  | Месторождения   | Рудные тела, залежи           |
| `RESOURCE_NODE`     | Узлы ресурсов   | Отдельные скопления ресурсов  |
| `EXTRACTION_METHOD` | Способы добычи  | Карьерная, шахтная            |
| `MINING_OPERATION`  | Операции добычи | Бурение, выемка               |
| `DRILLING_METHOD`   | Бурение         | Разведочное, эксплуатационное |
| `MINING_EQUIPMENT`  | Оборудование    | Буры, экскаваторы             |
| `GEOLOGICAL_SURVEY` | Геологоразведка | Сканирование, пробоотбор      |
| `RESOURCE_RESERVE`  | Запасы          | Объём, доступность            |
| `DEPLETION_RULE`    | Истощение       | Скорость расходования залежей |

Система добычи должна работать непосредственно с геологией. Месторождение не просто появляется по заранее заданным координатам: оно формируется из геологических процессов и состава слоёв.

## VIII. Industry & Engineering — промышленность и механизмы

[Minecraft Is Not What You Think, or: How I Learned to Stop Worrying and Love Polybenzimidazole | A cesspit.](https://images.openai.com/static-rsc-4/vDMzbw5ibObpVaFjsevOV94LDOADF7blub_XfelFeRyBjFLAC6Jt2Hf2uawa7OUOIIN15wWI6hILeJ08VINaBsIsce0wJgqM6B1aeGFIBcW9HZYKtnjDBan6Lpp9yW2wFMGIVgzjnD5FWBn0KQQBNMB206eNeSNPRWbe2vLYA9M?purpose=inline)

[Nomifactory CEu - Gallery - Minecraft Modpacks - CurseForge](https://images.openai.com/static-rsc-4/Ac9teXPpRelTA-3ARMtpvwv4yG84sZw9OmPO6AoLPWKBrtK5MEzMCbHm2__h37ZM-e9pbFpsZGNwrcum6m-PwYGIHbH93y76kbPkGyKNOtrsyeecSkHdCsSZGCT5SIf607mQ8Wj-wINm6MEzBURQogBiIdRGw5IpQ1rorYUiuHk?purpose=inline)

[Mechora - Precision Mechatronics & Industrial Automation](https://images.openai.com/static-rsc-4/WKGkzgtszFbCpMYAG0xfIMOfInHwlq-cO_vt29SnOwe6MYmJ42WQkG5eyKqncIcGjrcNS1zokCMd9EpLA_4eOEHIQaVo2kR8TBNLrvlnsHTaY3P_zcDe87L1GbxF2P-KJs9cApF7-4TJ1opGkjSuLqyJrPDICKtfvBaS51Rxdrk?purpose=inline)

11

Это второй центральный домен после Materials.

| RAW Type               | Содержание                 | Примеры                            |
| ---------------------- | -------------------------- | ---------------------------------- |
| `MACHINE`              | Машины                     | Дробилки, печи, центрифуги         |
| `MACHINE_TEMPLATE`     | Шаблоны машин              | Электрическая машина, механическая |
| `MACHINE_COMPONENT`    | Компоненты машин           | Двигатели, катушки, корпуса        |
| `MACHINE_PART`         | Детали                     | Шестерни, валы, подшипники         |
| `MECHANISM`            | Механизмы                  | Передачи, приводы, насосы          |
| `MECHANICAL_SYSTEM`    | Механические системы       | Редукторы, трансмиссии             |
| `PROCESSING_UNIT`      | Производственные агрегаты  | Мельницы, плавильни                |
| `MULTIBLOCK_STRUCTURE` | Многоблочные конструкции   | Большие заводские установки        |
| `RECIPE`               | Рецепты                    | Переработка материалов             |
| `REACTION`             | Производственные реакции   | Получение продукта                 |
| `PRODUCTION_PROCESS`   | Производственные процессы  | Обогащение, выплавка               |
| `FACTORY`              | Производственные комплексы | Заводы, фабрики                    |
| `PIPE`                 | Трубопроводы               | Трубы для жидкостей и газов        |
| `CONVEYOR`             | Транспортировка предметов  | Конвейерные системы                |
| `CABLE`                | Передача энергии           | Электрические кабели               |
| `CIRCUIT`              | Электрические цепи         | Контроллеры, логические цепи       |
| `AUTOMATION_RULE`      | Автоматизация              | Условия включения, сортировка      |
| `CONTROL_SYSTEM`       | Системы управления         | Регуляторы, контроллеры            |
| `INDUSTRIAL_STANDARD`  | Промышленные стандарты     | Допуски, классы оборудования       |

Здесь я бы не ограничивался системой «машина принимает X и выдаёт Y».

Машина должна иметь:

- Входные материальные потоки.
- Выходные материальные потоки.
- Потребление энергии.
- Рабочие режимы.
- Ограничения по температуре и давлению.
- Производительность.
- Износ.
- Требования к компонентам.
- Условия окружающей среды.
- Поддерживаемые процессы.

Например, центрифуга сможет работать с разными веществами, если их свойства и характеристики соответствуют требованиям процесса.

## IX. Biology — биология и организмы

[The Science Inside Every Creature : Generate Your Own Anatomy Renders - Gumvue.Studio](https://images.openai.com/static-rsc-4/Kw4xaTiyEqX3hm0BAr2lN0v_x_4KBAd5UtuatEKMRy6Gh7Nh6d9PN3QJzOuBHcejZpA2ughwE_ttm5O7iybGAU1ZVkIPNS8RqTnA3QU4JHCQc8kdCRU34XIndis14ieDJ8xTZbsvyzehcs-2R2_0c7dDdRQ0opHMFb2YyyEuiKM?purpose=inline)

[Bunte Bakterien und Mikroorganismen unter dem Mikroskop](https://images.openai.com/static-rsc-4/_ezGAKcXPE19DmESNy4qe6l6kfWH0IZ-I8QEZVB34yb6dXEOOqZKcQVbkE8uOMOheLYnmbJXxyGTG7aykrxEYtv_eloV8YKMtEYzQkUcIu8qxY9NbDatSCGYyntgVxRb57ylv1Yrqao9gxGkR5GWhGQICG_i0o95XcKFcdgL_dA?purpose=inline)

[Human Anatomy and Physiology +](https://images.openai.com/static-rsc-4/1-l1eTEQmOsIeGYxVfC6xuQKvbmLuE15a64Ip55uhgAfkDNaUwztpZW9Nu9pYzhhD1D9nJXNoiBYZiFezQWtVVmHojmyaRUIttDApDF4XK-TxaFib8hOZdGwzU1O6AZsoh0pjlh5WVFon5V7OjRLDsdkDjNT1cfYEdwOiHmx8JI?purpose=inline)

6

| RAW Type              | Содержание                   | Примеры                         |
| --------------------- | ---------------------------- | ------------------------------- |
| `SPECIES`             | Виды организмов              | Люди, волки, бактерии           |
| `ORGANISM`            | Общие определения организмов | Живые существа                  |
| `BODY`                | Строение тела                | Анатомические схемы             |
| `BODY_PART`           | Части тела                   | Органы, конечности              |
| `ORGAN`               | Органы                       | Сердце, лёгкие, печень          |
| `TISSUE`              | Ткани                        | Мышечная, нервная               |
| `CELL_TYPE`           | Типы клеток                  | Нейроны, эритроциты             |
| `MICROORGANISM`       | Микроорганизмы               | Бактерии, археи, грибы          |
| `GENOME`              | Генетические определения     | Гены, последовательности        |
| `GENETIC_TRAIT`       | Наследуемые признаки         | Пигментация, особенности обмена |
| `MUTATION`            | Мутации                      | Генетические изменения          |
| `METABOLISM`          | Метаболизм                   | Обмен веществ                   |
| `BIOCHEMICAL_PROCESS` | Биохимические процессы       | Синтез белков                   |
| `REPRODUCTION`        | Размножение                  | Половое, бесполое               |
| `LIFE_CYCLE`          | Жизненные циклы              | Стадии развития                 |
| `DISEASE`             | Заболевания                  | Инфекции, нарушения             |
| `IMMUNE_SYSTEM`       | Иммунные механизмы           | Иммунные реакции                |
| `BEHAVIOR`            | Поведение                    | Поиск пищи, миграция            |
| `INSTINCT`            | Инстинкты                    | Защитное, социальное поведение  |
| `SENSE`               | Органы чувств                | Зрение, слух, обоняние          |
| `PERSONALITY`         | Личностные характеристики    | Черты характера                 |
| `POPULATION`          | Популяции                    | Группы организмов               |

Особенно важный момент: человек здесь не должен быть отдельной жёстко прописанной системой.

Человек — это экземпляр `SPECIES`, у которого есть анатомия, генетика, физиология, метаболизм, поведение, когнитивные характеристики и социальные параметры.

То же самое относится к животным и микроорганизмам.

## X. Ecology — экология и биосфера

| RAW Type               | Содержание                 | Примеры                    |
| ---------------------- | -------------------------- | -------------------------- |
| `ECOSYSTEM`            | Экосистемы                 | Лесная, морская            |
| `FOOD_WEB`             | Пищевые сети               | Хищники, травоядные        |
| `ECOLOGICAL_ROLE`      | Экологические роли         | Производитель, потребитель |
| `HABITAT`              | Среды обитания             | Лес, океан, пещера         |
| `BIODIVERSITY`         | Биологическое разнообразие | Состав видов               |
| `SPECIES_INTERACTION`  | Взаимодействия видов       | Симбиоз, конкуренция       |
| `POPULATION_DYNAMICS`  | Динамика популяций         | Рождаемость, смертность    |
| `BIOGEOCHEMICAL_CYCLE` | Биогеохимические циклы     | Азотный, углеродный        |
| `POLLUTION`            | Загрязнение                | Химическое, радиоактивное  |
| `ENVIRONMENTAL_EVENT`  | Экологические события      | Засухи, вымирания          |

## XI. Technology & Research — технологии и научные исследования

[Kann Forschungstechnologie neue Entdeckungen ermöglichen 2025? | Vivo Magazin](https://images.openai.com/static-rsc-4/kNF6JQIcggy9xdVma84XqHpA7sHzZfm2eCZ48g8bxeS_z2kxH2FHc5s5DPcErXfEXszseRXJLD-WwnQ7aaCIvUrbmr57mCWumVKoasxLbZcnDyYsAIHaot8MDs8ih-jjNUvmONDHTmW8Ko7VjNv4te3V-CPL0lV7mQh7Nfik_g8?purpose=inline)

[Physics : College of Natural Sciences : UMass Amherst](https://images.openai.com/static-rsc-4/onFDsTQQXkDsrZ-llTilTJXQHLfqz_FH3dbC8qCY1cZIxZinlYG0Vpo1jx1ypvigFv-6mO3UBnV3F-w1D8dmT0R6PKQSFDMz8PfxD4nzJFttZlbTIWgBqlTDUbfsjIKIgInjtYdbKp3il4j8MCoS2tbvhYqGIa42v5TnN4mJjGA?purpose=inline)

[Timeline of Technology: Historical Milestones & Gadgets | AI Art Generator | Easy-Peasy.AI](https://images.openai.com/static-rsc-4/lLBGr2aiqwQmjCkyoI_2YhOmnok3w6NUbUY_48xA0MoK7tAuZWsASncIjCz0mME192e5q_6EcmdDJ104ta07LkF_9d0K4tRpPIYVpne5ruqggshMAkGB-TfAgQIZh0pkFdq1uPbLQb4QSnPKgwczzaL3Kx4wBNh2bSD_6wVxrCc?purpose=inline)

8

| RAW Type               | Содержание                | Примеры                          |
| ---------------------- | ------------------------- | -------------------------------- |
| `SCIENCE_FIELD`        | Научные дисциплины        | Физика, химия, биология          |
| `RESEARCH_FIELD`       | Направления исследований  | Ядерная физика, микробиология    |
| `TECHNOLOGY`           | Технологии                | Электричество, металлургия       |
| `TECHNOLOGY_CATEGORY`  | Категории технологий      | Энергетика, транспорт            |
| `TECHNOLOGY_ERA`       | Технологические эпохи     | Доиндустриальная, индустриальная |
| `RESEARCH_PROJECT`     | Исследовательские проекты | Изучение нового материала        |
| `RESEARCH_METHOD`      | Методы исследования       | Эксперимент, моделирование       |
| `DISCOVERY`            | Научные открытия          | Открытие элемента                |
| `TECHNOLOGY_UNLOCK`    | Разблокировки             | Новые рецепты, машины            |
| `RESEARCH_REQUIREMENT` | Требования исследований   | Оборудование, знания             |
| `SCIENTIFIC_THEORY`    | Научные теории            | Механика, эволюция               |
| `BLUEPRINT`            | Чертежи                   | Машины, конструкции              |
| `TECHNICAL_STANDARD`   | Технические стандарты     | Электрические стандарты          |
| `KNOWLEDGE`            | Знания                    | Изученные закономерности         |

Здесь очень важно разделить технологию, научное знание и эпоху.

Например:

- Физика — научная дисциплина.
- Электродинамика — направление исследований.
- Электромагнетизм — научное знание.
- Электрогенератор — технология.
- Электростанция — промышленное применение технологии.
- Электрификация — этап технологического развития цивилизации.

Одна технология может требовать знания сразу из нескольких научных дисциплин.

И одна эпоха может включать сотни технологий, но не обязательно должна быть жёстким линейным этапом.

## XII. Society & Culture — общества и культуры

| RAW Type             | Содержание             | Примеры                        |
| -------------------- | ---------------------- | ------------------------------ |
| `SAPIENT_SPECIES`    | Разумные виды          | Люди, другие разумные расы     |
| `CULTURE`            | Культуры               | Традиции, нормы, обычаи        |
| `CULTURAL_VALUE`     | Культурные ценности    | Свобода, честь, порядок        |
| `ETHIC`              | Этические нормы        | Отношение к собственности      |
| `SOCIAL_STRUCTURE`   | Социальные структуры   | Классы, касты, сословия        |
| `SOCIAL_CLASS`       | Общественные классы    | Рабочие, элиты                 |
| `SOCIAL_ROLE`        | Социальные роли        | Учёный, торговец               |
| `ORGANIZATION`       | Организации            | Гильдии, научные общества      |
| `RELIGION`           | Религиозные системы    | Религии, верования             |
| `IDEOLOGY`           | Идеологии              | Либерализм, социализм          |
| `LANGUAGE`           | Языки                  | Естественные и искусственные   |
| `EDUCATION_SYSTEM`   | Образование            | Школы, университеты            |
| `SOCIAL_INSTITUTION` | Институты общества     | Академии, суды                 |
| `DEMOGRAPHIC_GROUP`  | Демографические группы | Возрастные и социальные группы |

## XIII. Politics & Governance — политика и государства

[The Constitution of the Interstellar Commons — Consent, Rights, and Governance Across Species and Worlds - CyberNative.AI - Social Network with Interactive AI Agents](https://images.openai.com/static-rsc-4/sSKIw4ohVCWYuPlgVJZjaTfPNmMyXZ_Bn7ynVsn0De-udx8RnkGt2xJIgGMInNdVljS7vO8uaqlrJrN5lv7oct0_UFDpiftGU2jA81GMbs4pvDAcaYFcfRFOvoJVRqTiedxTsUh3BooOrR8B9PJAC0p0MHt5VTBgJLKCT71iyKg?purpose=inline)

[Circular map with various locations](https://images.openai.com/static-rsc-4/8_NdWBQUZ6zP1jt3HrAkRQBO5FD3l1lxHVu_YWf3p4t3R9EgbXZDxSwzKqV_AAzK8r6OifIQ0_QcHiDJnRbpb4J3KVEt7N_4-1zifhJCXzNY0LrH2b-XlXaqfJ5dImsU8rg-zWBFLiDMcR36zPsRqeQcaJ1VO96eob2ViJfTcRc?purpose=inline)

[Proclamation of War by King Ta'liote Myridos XIV of the Ka'yar Colonial Governance Document in The Delta Space | World Anvil](https://images.openai.com/static-rsc-4/i45gPPUOsxA0gEBwUBzN0oxy_o2ejRgcxCkaOcznoyJabwLVNpPkNC6BxXnOlatmAnBL4YM9M7o57MQwktAhPSO8SmprIODx2atzwKdv2YuK_DmIJPAbr3fsJ8rU23JNSOHlj5oudMQmJ6VfvWZhBvwUBcQVfdv6vx4cgLEubBY?purpose=inline)

10

| RAW Type                  | Содержание                | Примеры                  |
| ------------------------- | ------------------------- | ------------------------ |
| `STATE`                   | Государства               | Республика, империя      |
| `GOVERNMENT_TYPE`         | Формы правления           | Монархия, республика     |
| `POLITICAL_SYSTEM`        | Политические системы      | Демократия, авторитаризм |
| `IDEOLOGY`                | Политические идеологии    | Либерализм, консерватизм |
| `CONSTITUTION`            | Основные законы           | Конституции              |
| `LAW`                     | Законы                    | Налоги, собственность    |
| `POLICY`                  | Государственная политика  | Промышленная, научная    |
| `GOVERNMENT_POSITION`     | Государственные должности | Президент, министр       |
| `ADMINISTRATIVE_DIVISION` | Административные единицы  | Провинции, регионы       |
| `TERRITORIAL_CLAIM`       | Территориальные претензии | Притязания на территории |
| `DIPLOMATIC_RELATION`     | Дипломатические отношения | Союз, нейтралитет        |
| `TREATY`                  | Договоры                  | Торговые, политические   |
| `POLITICAL_MOVEMENT`      | Политические движения     | Реформаторские движения  |
| `ELECTION_SYSTEM`         | Избирательные системы     | Голосование              |
| `CITIZENSHIP`             | Гражданство               | Правовой статус          |

Государство я бы сделал полноценной сущностью, которая сама собирается из других определений.

Например, у государства будут ссылки на:

- Форму правления.
- Политическую систему.
- Идеологию.
- Конституцию.
- Законы.
- Административную структуру.
- Экономическую модель.
- Культуру.
- Государственные институты.
- Технологический уровень.
- Территории.
- Население.

## XIV. Economy & Trade — экономика и торговля

[Future economic globalization: multilateral or fragmented?  | World Economic Forum](https://images.openai.com/static-rsc-4/s1blAl1nNM5AADxTeRY-Wp4shUgHKNr1qq4P8_Fpc4Dk4sDiVWaWsRVy3kZ6R60FWHjJAQUvkyYMhgOqftLfjTxlhM6zloAvlUx5ECOIIgFxXwAHbkxKKiczGU6MlIYwrgC8eEbuiYaO5T_vLipnr7YcWXe3Nl6xJsGddrF0MjY?purpose=inline)

[B2B Marketplace Platform for Global Suppliers](https://images.openai.com/static-rsc-4/vHHIECngM2PvyxJEEvfFDi1yR-13hy58yfC9rjMBDKyEUninv-qcA1akDc9sC5ycpaANaBXAaT_dGyMDlCKshntnqen8cxHGIQbVLPtFzjGaSDPPfvzewBIfpBoP5ntVHKL1wp_3abY-uMnpn6CwSCNwGlLt1RgVjpGfMQDIpKs?purpose=inline)

[Lead & Bismuth Supply | Non-Ferrous Metals USA — Vigo Metal Trading](https://images.openai.com/static-rsc-4/2OpDuw2TOvoLn4MooMl0XUdhhN_rcIQbeAt2-_raz775fqUdztxDbCCGjB7Kxuq7jwIiwXWcTDoBm-5G_jjg0n1KdE4tTyi1l-vcEZQpSj_0brUhh846TzIMT-M35chntrFBOYySIg1OckD9c9667st8uinjwPyqjgcZHJQHTHU?purpose=inline)

9

| RAW Type            | Содержание                | Примеры                            |
| ------------------- | ------------------------- | ---------------------------------- |
| `ECONOMIC_SYSTEM`   | Экономические системы     | Рыночная, плановая                 |
| `ECONOMIC_MODEL`    | Экономические модели      | Смешанная экономика                |
| `ECONOMIC_GOOD`     | Экономические товары      | Сталь, зерно, электроэнергия       |
| `CURRENCY`          | Валюты                    | Денежные единицы                   |
| `MARKET`            | Рынки                     | Региональные, планетарные          |
| `COMMODITY`         | Биржевые товары           | Нефть, металлы                     |
| `PRICE_MODEL`       | Механизмы ценообразования | Спрос и предложение                |
| `TRADE_ROUTE`       | Торговые маршруты         | Межгородские, межпланетные         |
| `TRADE_AGREEMENT`   | Торговые соглашения       | Таможенные условия                 |
| `SUPPLY_CHAIN`      | Цепочки поставок          | Руда → металл → машина             |
| `PRODUCTION_SECTOR` | Сектора экономики         | Промышленность, сельское хозяйство |
| `TAX_SYSTEM`        | Налогообложение           | НДС, акцизы                        |
| `PROPERTY_SYSTEM`   | Собственность             | Частная, государственная           |
| `BANKING_SYSTEM`    | Банковская система        | Кредитование, депозиты             |
| `LABOR_MARKET`      | Рынок труда               | Занятость, заработная плата        |
| `ECONOMIC_POLICY`   | Экономическая политика    | Субсидии, тарифы                   |

Экономика должна быть связана с физическими ресурсами и реальным производством.

Например, сталь имеет физические свойства в `MATERIAL`, производственные рецепты в `REACTION`, промышленное применение в `MACHINE`, экономическую стоимость в `ECONOMIC_GOOD` и рыночную цену в `MARKET`.

Это одна и та же сущность материала, используемая разными системами.

## XV. Infrastructure & Transportation — инфраструктура и транспорт

| RAW Type                | Содержание            | Примеры                          |
| ----------------------- | --------------------- | -------------------------------- |
| `TRANSPORT_TYPE`        | Типы транспорта       | Наземный, воздушный, космический |
| `VEHICLE`               | Транспортные средства | Автомобили, корабли, ракеты      |
| `VEHICLE_COMPONENT`     | Компоненты транспорта | Двигатели, баки                  |
| `PROPULSION_SYSTEM`     | Двигательные системы  | Реактивные, электрические        |
| `ROAD_TYPE`             | Дороги                | Грунтовые, шоссе                 |
| `RAIL_SYSTEM`           | Железные дороги       | Рельсы, локомотивы               |
| `PORT`                  | Порты                 | Морские, космические             |
| `POWER_GRID`            | Энергосети            | Электрические сети               |
| `WATER_NETWORK`         | Водоснабжение         | Водопроводы                      |
| `COMMUNICATION_NETWORK` | Связь                 | Радио, спутниковая связь         |
| `SPACE_STATION`         | Космические станции   | Орбитальные комплексы            |
| `SETTLEMENT`            | Поселения             | Города, колонии                  |
| `INFRASTRUCTURE`        | Общая инфраструктура  | Сети и сооружения                |

# 3. Универсальные системы, которые должны объединять все домены

Вот здесь, на мой взгляд, находится самое важное архитектурное решение.

Не нужно создавать 15 совершенно независимых систем, каждая из которых имеет собственную реализацию свойств, рецептов, процессов и взаимодействий.

Нужно сделать несколько универсальных data-driven механизмов, которыми пользуются все 15 доменов.

Я бы выделил следующие.

## 3.1. Universal Entity System

Это основа всего.

Каждая сущность игры должна иметь собственное определение и возможность включать другие определения.

Например:

| Сущность            | Из чего собирается                                        |
| ------------------- | --------------------------------------------------------- |
| Планета             | Планетарный шаблон + слои + атмосфера + геология          |
| Железная руда       | Минеральный состав + геологическое происхождение          |
| Сталь               | Материал + химический состав + физические свойства        |
| Человек             | Вид + анатомия + генетика + физиология                    |
| Бактерия            | Вид + клеточная структура + метаболизм                    |
| Печь                | Машина + компоненты + производственные процессы           |
| Государство         | Политическая система + законы + территория + население    |
| Завод               | Структура + машины + логистика + производственные цепочки |
| Космический корабль | Конструкция + компоненты + двигатель + энергетика         |

Я бы предусмотрел следующие универсальные RAW-типы:

| RAW Type           | Назначение                                                      |
| ------------------ | --------------------------------------------------------------- |
| `ENTITY_TEMPLATE`  | Универсальный шаблон сущности                                   |
| `ENTITY_COMPONENT` | Компонент сущности                                              |
| `ENTITY_TRAIT`     | Характеристика сущности                                         |
| `ENTITY_CLASS`     | Категория сущности                                              |
| `ENTITY_VARIATION` | Вариация сущности                                               |
| `ENTITY_INSTANCE`  | Определение конкретного экземпляра, если оно нужно для контента |
| `ENTITY_RELATION`  | Связи между сущностями                                          |
| `ENTITY_STATE`     | Состояния сущностей                                             |
| `ENTITY_EVENT`     | События, воздействующие на сущности                             |
| `ENTITY_MODIFIER`  | Модификаторы характеристик                                      |

Принцип: сущность не обязана иметь уникальный набор характеристик. Она получает их через подключаемые компоненты.

Например, у машины есть `PHYSICAL_PROPERTIES`, `ENERGY_REQUIREMENTS`, `INPUTS`, `OUTPUTS`, `DURABILITY`.

У человека — `BODY`, `GENOME`, `METABOLISM`, `PERSONALITY`, `SENSES`.

Общие механизмы одни, компоненты разные.

## 3.2. Universal Property System

Система характеристик.

| RAW Type              | Назначение           | Примеры                         |
| --------------------- | -------------------- | ------------------------------- |
| `PROPERTY`            | Определение свойства | Масса, температура              |
| `PROPERTY_TYPE`       | Тип свойства         | Числовое, категориальное        |
| `PROPERTY_UNIT`       | Единицы измерения    | кг, Па, К                       |
| `PROPERTY_RANGE`      | Допустимые значения  | Минимум, максимум               |
| `PROPERTY_FORMULA`    | Расчётные свойства   | Плотность, КПД                  |
| `PROPERTY_MODIFIER`   | Изменение свойства   | Температурный модификатор       |
| `PROPERTY_DEPENDENCY` | Зависимости          | Давление зависит от температуры |

Это позволит не прописывать массу, плотность и температуру отдельным образом для каждой категории.

Именно через эту систему можно реализовать практически любую физическую характеристику.

## 3.3. Universal Process System

Одна из важнейших систем

Вместо создания отдельных несовместимых механизмов для химии, металлургии, биологии и геологии я бы сделал общий тип `PROCESS`.

| RAW Type                 | Назначение         |
| ------------------------ | ------------------ |
| `PROCESS`                | Общий процесс      |
| `PROCESS_INPUT`          | Входные компоненты |
| `PROCESS_OUTPUT`         | Результаты         |
| `PROCESS_CONDITION`      | Условия выполнения |
| `PROCESS_PARAMETER`      | Параметры          |
| `PROCESS_STAGE`          | Стадии             |
| `PROCESS_TRANSFORMATION` | Изменение объектов |
| `PROCESS_REQUIREMENT`    | Требования         |
| `PROCESS_CATALYST`       | Катализаторы       |
| `PROCESS_BYPRODUCT`      | Побочные продукты  |
| `PROCESS_RATE`           | Скорость           |
| `PROCESS_LIMIT`          | Ограничения        |

Примеры использования:

| Процесс                | Что происходит                                     |
| ---------------------- | -------------------------------------------------- |
| Плавление              | Твёрдый материал → жидкий                          |
| Кристаллизация         | Жидкость → кристаллическое твёрдое тело            |
| Окисление              | Химическое преобразование                          |
| Дробление              | Изменение формы материала                          |
| Электролиз             | Химическое преобразование с электрической энергией |
| Фотосинтез             | Биологический процесс                              |
| Метаболизм             | Биохимические преобразования                       |
| Осадконакопление       | Геологическое образование слоя                     |
| Тектоническое движение | Изменение геологического состояния                 |
| Дистилляция            | Разделение смеси                                   |
| Электрогенерация       | Преобразование энергии                             |

Таким образом, игра будет иметь единый механизм исполнения процессов, а различаться будут их определения.

## 3.4. Universal Relationship System

Сущности должны уметь ссылаться друг на друга.

| RAW Type            | Назначение                         |
| ------------------- | ---------------------------------- |
| `RELATION_TYPE`     | Типы связей                        |
| `DEPENDENCY`        | Зависимости                        |
| `COMPOSITION`       | Состав                             |
| `CONTAINMENT`       | Содержание одной сущности в другой |
| `OWNERSHIP`         | Владение                           |
| `PRODUCTION_LINK`   | Производственные связи             |
| `CAUSAL_LINK`       | Причинно-следственные связи        |
| `SPATIAL_RELATION`  | Пространственные отношения         |
| `TEMPORAL_RELATION` | Временные отношения                |

Примеры:

- Планета состоит из слоёв.
- Руда содержит минералы.
- Минерал состоит из химических соединений.
- Машина состоит из компонентов.
- Завод содержит машины.
- Государство контролирует территории.
- Организм состоит из органов.
- Технология требует определённых научных знаний.

## 3.5. Universal Simulation Rules

Отдельная система правил симуляции.

| RAW Type               | Назначение              |
| ---------------------- | ----------------------- |
| `RULE`                 | Общее правило           |
| `RULE_SET`             | Набор правил            |
| `CONSTRAINT`           | Ограничения             |
| `CONDITION`            | Условия                 |
| `TRIGGER`              | Триггеры                |
| `EVENT`                | События                 |
| `EVENT_HANDLER`        | Реакция на события      |
| `SIMULATION_MODEL`     | Модель симуляции        |
| `SIMULATION_PARAMETER` | Параметры симуляции     |
| `SCHEDULE`             | Периодичность процессов |

Например, изменение климата может запускать гидрологические процессы, которые влияют на эрозию, которая изменяет геологию, которая изменяет доступность ресурсов.

Не нужно прописывать такую цепочку в каждом отдельном объекте: связи и правила определяются данными.

# 4. Как должны выглядеть сами определения сущностей

Теперь самое интересное: я бы сделал формат определений максимально похожим на RAW Dwarf Fortress, но расширил его.

Например, представим, что мы хотим добавить новый материал — условный титановый сплав.

Его определение не должно содержать вообще всю логику игры. Оно должно ссылаться на другие определения.

Условный пример:

```
[OBJECT:MATERIAL]

[MATERIAL:ADVANCED_TITANIUM_ALLOY]
    [NAME:Advanced Titanium Alloy]
    [CLASS:ALLOY]
    [COMPOSITION:TITANIUM:90]
    [COMPOSITION:ALUMINUM:6]
    [COMPOSITION:VANADIUM:4]

    [PROPERTY:DENSITY:4430]
    [PROPERTY:HARDNESS:HIGH]
    [PROPERTY:MELTING_POINT:1900]
    [PROPERTY:THERMAL_CONDUCTIVITY:HIGH]
    [PROPERTY:ELECTRICAL_CONDUCTIVITY:HIGH]

    [PHASE:SOLID]
    [PROCESSABLE:CASTING]
    [PROCESSABLE:MACHINING]
    [PROCESSABLE:FORGING]

    [RESEARCH_REQUIREMENT:ADVANCED_METALLURGY]
    [TECHNOLOGY_REQUIREMENT:INDUSTRIAL_METALLURGY]
```

Это иллюстративный синтаксис, не готовый формат какого-либо существующего движка.

А теперь представим, что мы создаём машину.

```
[OBJECT:MACHINE]

[MACHINE:INDUSTRIAL_ELECTRIC_FURNACE]
    [NAME:Industrial Electric Furnace]

    [CLASS:FURNACE]
    [COMPONENT:HEATING_ELEMENT]
    [COMPONENT:INDUSTRIAL_CONTROLLER]
    [COMPONENT:INSULATION]

    [PROCESS:SMELTING]
    [PROCESS:ALLOYING]
    [PROCESS:HEAT_TREATMENT]

    [ENERGY_TYPE:ELECTRICAL]
    [ENERGY_INPUT:5000]

    [PROPERTY:MAX_TEMPERATURE:2500]
    [PROPERTY:EFFICIENCY:0.85]

    [REQUIRES_TECHNOLOGY:INDUSTRIAL_ELECTRICITY]
```

И наконец, планета:

```
[OBJECT:PLANET]

[PLANET:PLANET_EXAMPLE]
    [CLASS:TERRESTRIAL]

    [PROPERTY:MASS:...]
    [PROPERTY:RADIUS:...]
    [PROPERTY:GRAVITY:...]

    [LAYER:CORE]
    [LAYER:MANTLE]
    [LAYER:CRUST]

    [ATMOSPHERE:EARTH_LIKE]
    [HYDROSPHERE:OCEANIC]

    [TECTONICS:ACTIVE]
    [CLIMATE_MODEL:PLANETARY_CLIMATE]

    [BIOME_GENERATION:ENABLED]
    [GEOLOGY_GENERATION:ENABLED]
    [MINERAL_GENERATION:ENABLED]
```

В реальности я бы ещё отделил шаблоны, экземпляры и генерируемые параметры, чтобы не смешивать описание планеты с конкретной планетой, появившейся в мире.

# 5. Система технологических эпох и исследований

Я бы сделал её отдельным крупным доменом, потому что она будет связывать практически все остальные.

[Alt-F4 #14 - Exploring the Infinite](https://images.openai.com/static-rsc-4/84QGwgVVflIj5ypXQumUJSOjHAgs4RzfXJo4ZNvnFHbPSlTT-Ilo76jDnkcyge3Eg1VN0fgyGTKNod7Mt1snlMOf3msU2Ipdq0eOqE_IBc-gVZ3uD5dwGWN_v-MDv5XXuHtbtwObzeMCgTamkzGmPHi-IyZjUJmQxFFVo2exv4c?purpose=inline)

[alt-f4.blog](https://alt-f4.blog/ALTF4-14/?utm_source=chatgpt.com)

Предлагаемая структура:

| RAW Type                | Что определяет           |
| ----------------------- | ------------------------ |
| `ERA`                   | Технологические эпохи    |
| `TECHNOLOGY`            | Конкретные технологии    |
| `TECHNOLOGY_BRANCH`     | Ветки развития           |
| `SCIENCE_FIELD`         | Научные дисциплины       |
| `RESEARCH_FIELD`        | Направления исследований |
| `KNOWLEDGE`             | Научные знания           |
| `DISCOVERY`             | Открытия                 |
| `RESEARCH_PROJECT`      | Проекты                  |
| `RESEARCH_METHOD`       | Методы исследований      |
| `RESEARCH_REQUIREMENT`  | Требования               |
| `TECHNOLOGY_DEPENDENCY` | Зависимости              |
| `TECHNOLOGY_UNLOCK`     | Разблокировки            |
| `RESEARCH_INSTITUTION`  | Научные учреждения       |

Пример эпох:

| Эпоха                  | Примерные технологии                                             |
| ---------------------- | ---------------------------------------------------------------- |
| Доиндустриальная       | Земледелие, металлургия, механика                                |
| Ранняя индустриальная  | Паровые машины, угольная промышленность                          |
| Поздняя индустриальная | Электричество, химическая промышленность                         |
| Электротехническая     | Электродвигатели, генераторы                                     |
| Атомная                | Ядерная физика, реакторы                                         |
| Информационная         | Электроника, вычислительные машины                               |
| Космическая            | Ракеты, орбитальная инфраструктура                               |
| Межпланетная           | Колонизация планет, космическая добыча                           |
| Продвинутая научная    | Новые материалы, биотехнологии, передовые энергетические системы |

Но эпохи должны быть скорее классификацией, чем обязательной последовательностью.

Например, цивилизация может развить химию и металлургию, но не открыть определённые направления электроники.

А другая цивилизация может получить доступ к некоторым технологиям через торговлю или научный обмен.

То есть технология должна иметь:

- Принадлежность к эпохе.
- Научные предпосылки.
- Необходимые знания.
- Необходимое оборудование.
- Производственные предпосылки.
- Возможные альтернативные способы открытия.
- Разблокируемые технологии.
- Разблокируемые рецепты.
- Разблокируемые машины.
- Разблокируемые материалы.

# 6. Система форм материалов и предметов

Здесь я бы позаимствовал подход Minecraft и GregTech, но сделал его более универсальным.

Материал не должен быть привязан к конкретной форме.

[Shera Energy Limited](https://images.openai.com/static-rsc-4/496ICTlmqMILszpjueyi08ZpJcWlbQbdOnBaynU4sYNMOx8QlkMuzIN4PzSSTBzTlRAkGHd2Y58IRYJdxM0JUXJYySXtw5f-2U6cAUiVAbX_7ffvCj5Ft8hsWEsA9t0OLYkIoySRtK3RYm0nKzEAGQsP0lFDCOYhBjVQtaE2sWs?purpose=inline)

[Hydrometallurgical Recycling of Critical Raw Materials Course - Activities - Dutch Process Technologists NPT - Specialist departments - Communities - KIVI](https://images.openai.com/static-rsc-4/7q_K4bSe8cVQljZyKnJKTN-bhykOG_SXU_-WfxaNdY1Qd9VsUe_iaUXf4jkbiHVE5vVVrsmNBxN422tL8ie-OlAec875DZU7d9J9mhZ-fPlDkHbmLPt4fszya5Ub7C4-jwYNvtCcb9Wc5iMghtcA4SL-YScrm0a7r7hcyf-6cvQ?purpose=inline)

[Talmech Trading | Verified Metal Marketplace in India](https://images.openai.com/static-rsc-4/KXEVq7yTJ-eWbxQht0YLT0TDU3Ax6HlNvGhdiCNO3k8dqyEBIEh3Aun56W6zioxAbut72sI1YkH9HtNXkEyrkNvbmFUvnyJGBx1PXyHC6JXkGGohQLGslxOrCnoD2zRGNfcJVbWjrUethCGhYM0QdKyqKQU7p2iyMuk5vBGxXSA?purpose=inline)

10

Например, у стали могут существовать следующие формы:

| RAW Type        | Примеры                        |
| --------------- | ------------------------------ |
| `MATERIAL_FORM` | Универсальные формы материалов |
| `BLOCK`         | Блок                           |
| `INGOT`         | Слиток                         |
| `NUGGET`        | Самородок                      |
| `DUST`          | Пыль                           |
| `FINE_DUST`     | Мелкодисперсная пыль           |
| `PLATE`         | Пластина                       |
| `ROD`           | Стержень                       |
| `WIRE`          | Проволока                      |
| `FOIL`          | Фольга                         |
| `SHEET`         | Лист                           |
| `GEAR`          | Шестерня                       |
| `PIPE`          | Труба                          |
| `FRAME`         | Каркас                         |
| `BOLT`          | Болт                           |
| `SCREW`         | Винт                           |
| `CABLE`         | Кабель                         |
| `FLUID`         | Жидкая форма                   |
| `GAS`           | Газообразная форма             |

Я бы не создавал для каждой формы отдельный материал.

Вместо этого:

```
[MATERIAL:STEEL]

[COMPOSITION:IRON:...]
[COMPOSITION:CARBON:...]

[FORM:BLOCK]
[FORM:INGOT]
[FORM:PLATE]
[FORM:ROD]
[FORM:WIRE]
[FORM:GEAR]
[FORM:PIPE]
```

А для каждой формы можно задать собственные характеристики:

- Размеры.
- Массу.
- Объём.
- Геометрию.
- Производственные методы.
- Допустимые процессы.
- Требования к оборудованию.

Например, стальная проволока и стальная пластина имеют одинаковый базовый материал, но совершенно разные формы и области применения.

Также я бы разделил:

1. Material — из чего сделано.
2. Form — в какой форме существует.
3. Item — конкретный тип игрового объекта.
4. Item Instance — конкретный экземпляр объекта.

Это очень важное разделение для масштабируемости.

# 7. Ещё несколько универсальных доменов

Есть категории, которые не стоит забывать, хотя они не являются центром твоей концепции.

| Домен          | RAW-типы                                                     | Примеры                                      |
| -------------- | ------------------------------------------------------------ | -------------------------------------------- |
| Время          | `TIME_UNIT`, `CALENDAR`, `TIME_SCALE`                        | Секунды, годы, планетарные календари         |
| Пространство   | `COORDINATE_SYSTEM`, `REGION`, `LOCATION`                    | Координаты, области, точки                   |
| Графика        | `GRAPHICS`, `TILE`, `MODEL`, `TEXTURE`, `PALETTE`            | Отображение материалов, существ, машин       |
| Звук           | `SOUND`, `SOUND_EVENT`, `MUSIC`                              | Звуковые эффекты                             |
| Язык           | `LANGUAGE`, `WORD`, `NAMING_RULE`                            | Названия материалов, государств              |
| История        | `HISTORICAL_EVENT`, `HISTORICAL_FIGURE`, `HISTORICAL_RECORD` | История цивилизаций                          |
| Взаимодействия | `INTERACTION`, `EFFECT`, `TRIGGER`                           | Взаимодействие объектов                      |
| Безопасность   | `HAZARD`, `RISK`, `PROTECTION`                               | Радиация, токсичность, защита                |
| Инструменты    | `TOOL`, `TOOL_CATEGORY`                                      | Измерительные и производственные инструменты |
| Стандарты      | `STANDARD`, `SPECIFICATION`                                  | Единицы, допуски, совместимость              |

# 8. Как бы я организовал RAW-файлы

Я бы сделал примерно такую структуру каталогов.

```
/data
│
├── /core
│   ├── entity_types
│   ├── properties
│   ├── units
│   ├── relationships
│   ├── processes
│   ├── events
│   └── simulation_rules
│
├── /cosmology
│   ├── stars
│   ├── star_classes
│   ├── celestial_bodies
│   ├── planets
│   ├── orbits
│   └── astrophysics
│
├── /planetology
│   ├── planet_templates
│   ├── planetary_layers
│   ├── tectonics
│   ├── atmospheres
│   └── planetary_formation
│
├── /geoscience
│   ├── biomes
│   ├── terrain
│   ├── geology
│   ├── minerals
│   ├── soils
│   ├── climates
│   └── hydrology
│
├── /materials
│   ├── elements
│   ├── isotopes
│   ├── compounds
│   ├── alloys
│   ├── materials
│   ├── properties
│   ├── forms
│   └── templates
│
├── /physics
│   ├── mechanics
│   ├── thermodynamics
│   ├── electromagnetism
│   ├── radiation
│   └── energy
│
├── /chemistry
│   ├── reactions
│   ├── catalysts
│   ├── solutions
│   ├── mixtures
│   └── processes
│
├── /resources
│   ├── deposits
│   ├── extraction
│   ├── mining
│   └── surveying
│
├── /industry
│   ├── machines
│   ├── components
│   ├── mechanisms
│   ├── recipes
│   ├── production
│   ├── multiblocks
│   └── automation
│
├── /biology
│   ├── species
│   ├── anatomy
│   ├── tissues
│   ├── cells
│   ├── genetics
│   ├── metabolism
│   ├── diseases
│   └── behavior
│
├── /ecology
│   ├── ecosystems
│   ├── habitats
│   ├── food_webs
│   └── interactions
│
├── /technology
│   ├── eras
│   ├── technologies
│   ├── science_fields
│   ├── research
│   ├── discoveries
│   └── blueprints
│
├── /society
│   ├── cultures
│   ├── ideologies
│   ├── social_structures
│   ├── religions
│   └── institutions
│
├── /politics
│   ├── states
│   ├── governments
│   ├── laws
│   ├── policies
│   └── diplomacy
│
├── /economy
│   ├── economic_systems
│   ├── goods
│   ├── currencies
│   ├── markets
│   ├── trade
│   └── production_sectors
│
└── /infrastructure
    ├── transportation
    ├── vehicles
    ├── networks
    ├── settlements
    └── space_infrastructure
```

Это логическая организация, а не требование хранить каждый объект в отдельном файле. Внутри папок можно группировать определения по пакетам.

# 9. Как всё это должно взаимодействовать

Вот пример того, как один материал проходит через практически все системы.

## Пример: от формирования планеты до промышленности

1. Cosmology

   Формируется планетарная система, появляется каменистая планета.
2. Planetology

   Происходит дифференциация планеты, формируются кора и мантия.
3. Geoscience

   Геологические процессы создают минеральные образования и месторождения.
4. Materials

   Минералы содержат химические соединения и определённые элементы.
5. Resources

   Цивилизация обнаруживает месторождение и начинает добычу.
6. Chemistry

   Руда проходит обогащение, разделение и химическую переработку.
7. Physics & Energy

   Для переработки используются тепловая и электрическая энергия.
8. Industry

   Машины перерабатывают сырьё в металл, сплавы и компоненты.
9. Technology

   Открытия позволяют создавать более эффективные машины и процессы.
10. Economy

    Материалы поступают в производство, торговлю и государственные запасы.
11. Politics

    Государство регулирует добычу, промышленность, торговлю и исследования.

Самое замечательное: тебе не нужно делать отдельную систему для каждого подобного сценария. Он возникает из взаимодействия универсальных систем.

# 10. Что я бы добавил в архитектуру обязательно

Есть пять решений, которые я считаю особенно важными.

| Решение                  | Почему необходимо                                                                |
| ------------------------ | -------------------------------------------------------------------------------- |
| Наследование шаблонов    | Не придётся каждый раз создавать материал или организм с нуля                    |
| Компонентная архитектура | Можно собирать сложные сущности из независимых частей                            |
| Единая система свойств   | Физические, химические, биологические и экономические параметры будут совместимы |
| Универсальные процессы   | Одни механизмы смогут обслуживать химию, промышленность, геологию и биологию     |
| Система зависимостей     | Можно создавать сложные технологические цепочки и взаимосвязанные объекты        |

Ещё три важных технических решения:

- Версионирование RAW и миграции определений.
- Валидация ссылок и совместимости сущностей при загрузке.
- Отделение определений от сгенерированных экземпляров мира.

# 11. Самое важное: не пытаться симулировать абсолютно всё одинаково подробно

У твоей игры потенциально колоссальный масштаб. Поэтому я бы разделил симуляцию на три уровня.

| Уровень      | Что симулируется                                           |
| ------------ | ---------------------------------------------------------- |
| Детальный    | Машины, химические реакции, организмы, локальные процессы  |
| Региональный | Экономика, экосистемы, население, геологические изменения  |
| Макроуровень | Планетарная эволюция, государства, космические цивилизации |

Например, планета может содержать миллионы геологических участков, но не обязательно каждый из них должен обрабатываться с одинаковой частотой и детализацией.

При этом определения материалов, организмов, процессов и технологий остаются общими на всех уровнях.

# 12. Моя итоговая рекомендация: какой набор RAW Object Types я бы заложил

Если бы я проектировал фундамент игры, я бы не создавал сотни отдельных верхнеуровневых типов без необходимости. Я бы сделал примерно 60–75 базовых типов определений, а всё остальное реализовал бы через компоненты, свойства, шаблоны, токены и связи.

Вот мой основной список:

| №  | RAW Object Type     | Основная область          |
| -- | ------------------- | ------------------------- |
| 1  | `ENTITY_TEMPLATE`   | Универсальная архитектура |
| 2  | `ENTITY_COMPONENT`  | Универсальная архитектура |
| 3  | `ENTITY_CLASS`      | Универсальная архитектура |
| 4  | `ENTITY_TRAIT`      | Универсальная архитектура |
| 5  | `ENTITY_RELATION`   | Универсальная архитектура |
| 6  | `ENTITY_STATE`      | Универсальная архитектура |
| 7  | `PROPERTY`          | Свойства                  |
| 8  | `PROPERTY_TYPE`     | Свойства                  |
| 9  | `PROPERTY_FORMULA`  | Свойства                  |
| 10 | `UNIT`              | Измерения                 |
| 11 | `PROCESS`           | Процессы                  |
| 12 | `PROCESS_TEMPLATE`  | Процессы                  |
| 13 | `PROCESS_STAGE`     | Процессы                  |
| 14 | `REACTION`          | Реакции                   |
| 15 | `EVENT`             | События                   |
| 16 | `RULE`              | Правила                   |
| 17 | `SIMULATION_MODEL`  | Симуляция                 |
| 18 | `STAR`              | Космология                |
| 19 | `STELLAR_SYSTEM`    | Космология                |
| 20 | `CELESTIAL_BODY`    | Космология                |
| 21 | `ORBIT`             | Космология                |
| 22 | `PLANET_TEMPLATE`   | Планетология              |
| 23 | `PLANET_LAYER`      | Планетология              |
| 24 | `TECTONIC_PLATE`    | Планетология              |
| 25 | `ATMOSPHERE`        | Планетология              |
| 26 | `HYDROSPHERE`       | Планетология              |
| 27 | `BIOME`             | География                 |
| 28 | `TERRAIN`           | География                 |
| 29 | `GEOLOGICAL_LAYER`  | Геология                  |
| 30 | `ROCK_TYPE`         | Геология                  |
| 31 | `MINERAL`           | Геология                  |
| 32 | `DEPOSIT`           | Геология                  |
| 33 | `CLIMATE`           | Климат                    |
| 34 | `WATER_BODY`        | Гидрология                |
| 35 | `ELEMENT`           | Материалы                 |
| 36 | `ISOTOPE`           | Материалы                 |
| 37 | `COMPOUND`          | Материалы                 |
| 38 | `MATERIAL`          | Материалы                 |
| 39 | `MATERIAL_TEMPLATE` | Материалы                 |
| 40 | `MATERIAL_FORM`     | Материалы                 |
| 41 | `ALLOY`             | Материалы                 |
| 42 | `FLUID`             | Материалы                 |
| 43 | `GAS`               | Материалы                 |
| 44 | `ENERGY_TYPE`       | Физика                    |
| 45 | `ENERGY_SOURCE`     | Энергетика                |
| 46 | `ENERGY_STORAGE`    | Энергетика                |
| 47 | `FIELD_TYPE`        | Физика                    |
| 48 | `SPECIES`           | Биология                  |
| 49 | `BODY`              | Биология                  |
| 50 | `ORGAN`             | Биология                  |
| 51 | `TISSUE_TEMPLATE`   | Биология                  |
| 52 | `CELL_TYPE`         | Биология                  |
| 53 | `GENOME`            | Биология                  |
| 54 | `GENETIC_TRAIT`     | Биология                  |
| 55 | `METABOLISM`        | Биология                  |
| 56 | `BEHAVIOR`          | Биология                  |
| 57 | `ECOSYSTEM`         | Экология                  |
| 58 | `MACHINE`           | Промышленность            |
| 59 | `MACHINE_COMPONENT` | Промышленность            |
| 60 | `MECHANISM`         | Промышленность            |
| 61 | `RECIPE`            | Производство              |
| 62 | `BUILDING`          | Инфраструктура            |
| 63 | `ITEM`              | Предметы                  |
| 64 | `TECHNOLOGY`        | Технологии                |
| 65 | `ERA`               | Технологии                |
| 66 | `SCIENCE_FIELD`     | Наука                     |
| 67 | `KNOWLEDGE`         | Наука                     |
| 68 | `RESEARCH_PROJECT`  | Наука                     |
| 69 | `CULTURE`           | Общество                  |
| 70 | `IDEOLOGY`          | Общество                  |
| 71 | `SOCIAL_STRUCTURE`  | Общество                  |
| 72 | `STATE`             | Политика                  |
| 73 | `GOVERNMENT_TYPE`   | Политика                  |
| 74 | `LAW`               | Политика                  |
| 75 | `ORGANIZATION`      | Общество                  |
| 76 | `ECONOMIC_SYSTEM`   | Экономика                 |
| 77 | `ECONOMIC_GOOD`     | Экономика                 |
| 78 | `CURRENCY`          | Экономика                 |
| 79 | `MARKET`            | Экономика                 |
| 80 | `TRADE_ROUTE`       | Экономика                 |
| 81 | `VEHICLE`           | Транспорт                 |
| 82 | `PROPULSION_SYSTEM` | Транспорт                 |
| 83 | `INFRASTRUCTURE`    | Инфраструктура            |
| 84 | `SETTLEMENT`        | Инфраструктура            |
| 85 | `LANGUAGE`          | Язык                      |
| 86 | `GRAPHICS`          | Визуализация              |
| 87 | `SOUND`             | Звук                      |
| 88 | `HISTORICAL_EVENT`  | История                   |

88 базовых типов в предлагаемом каталоге

Это не окончательный обязательный стандарт: часть типов можно объединить, а часть выделить позже. Например, `MINERAL` и `ROCK_TYPE` можно реализовать через общую систему материалов с дополнительными геологическими компонентами.

И вот какой принцип я бы считал главным для твоей игры:

Не создавать отдельную сущность с отдельной механикой для каждого минерала, организма, машины, государства или технологии. Создавать универсальные определения, из которых эти сущности собираются.

Тогда ты получишь не просто огромную базу контента, а действительно расширяемую симуляционную платформу, в которой появление нового материала может повлиять на химию, производство, экономику и технологическое развитие цивилизации, а появление новой планеты — на геологию, биосферу, ресурсы и историю её обитателей.