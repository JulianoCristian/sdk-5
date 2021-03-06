// Copyright (c) 2016, the Dartino project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

library stm32.ethernet;

import 'dart:dartino.ffi';
import 'package:os/os.dart' as os show InternetAddress;

final _initializeNetwork =
  ForeignLibrary.main.lookup("InitializeNetworkStack");
final _isNetworkUp = ForeignLibrary.main.lookup("IsNetworkUp");
final _getEthernetAdapterStatus =
  ForeignLibrary.main.lookup("GetEthernetAdapterStatus");
final _getNetworkAddressConfiguration =
  ForeignLibrary.main.lookup("GetNetworkAddressConfiguration");
final _lookupHost = ForeignLibrary.main.lookup("network_lookup_host");
final _networkAddressMayHaveChanged = ForeignLibrary.main.lookup(
    "NetworkAddressMayHaveChanged");

Ethernet ethernet = new Ethernet._internal();

// TODO(karlklose): use the one in os.dart?
class InternetAddress implements os.InternetAddress {
  final List<int> bytes;

  const InternetAddress(this.bytes);

  toString() => bytes.join('.');

  bool get isIP4 => bytes.length == 4;

  static final InternetAddress localhost =
    const InternetAddress(const <int>[127, 0, 0, 1]);
}

// TODO(karlklose): rename to internet, ipStack, ...?
class Ethernet {
  Ethernet._internal();

  /// Initialize ethernet hardware and software stack.
  ///
  /// If compiled with DHCP support (the default), the stack will start to
  /// request a configuration.  If a DHCP configuration is received in the
  /// timeout window, that configuration will be used.  Otherwise, the stack
  /// will be configured with the provided parameters.
  ///
  /// (There is currently no way to enable or disable DHCP from Dart)
  ///
  /// This method returns `true` if and only if the stack could be initialized
  /// successfully.
  ///
  /// This does not mean that the ethernet adapter is usable directly after the
  /// method returns `true` (for example, if there is delay in the DHCP
  /// configuration process).  The result of [NetworkInterface.list] should be
  /// used to determine whether it is.
  ///
  /// It is an error to call this method more than once, even with the same
  /// arguments.
  bool initializeNetworkStack(InternetAddress address, InternetAddress netmask,
      InternetAddress gateway, InternetAddress dnsServer) {
    if (_initialized) {
      throw new StateError("network stack already initialized");
    }
    ForeignMemory configuration = new ForeignMemory.allocated(16);
    try {
      _writeIp4Address(configuration, 0, address);
      _writeIp4Address(configuration, 4, netmask);
      _writeIp4Address(configuration, 8, gateway);
      _writeIp4Address(configuration, 12, dnsServer);
      _initialized = (_initializeNetwork.icall$1(configuration.address) == 1);
      _lo = new _NetworkInterface(_NetworkInterface.LO_INTERFACE_INDEX,
          "lo",
          <InternetAddress>[InternetAddress.localhost]);
      _eth = new _NetworkInterface(_NetworkInterface.ETH_INTERFACE_INDEX,
          "eth0",
          <InternetAddress>[]);
      return _initialized;
    } finally {
      configuration.free();
    }
  }

  InternetAddress lookup(String name) {
    ForeignMemory string = new ForeignMemory.fromStringAsUTF8(name);
    ForeignMemory address = new ForeignMemory.allocated(4);
    try {
      int success = _lookupHost.icall$2(string.address, address.address);
      if (success == 0) {
        return null;
      }
      List<int> bytes = new List<int>(4);
      address.copyBytesToList(bytes, 0, 4, 0);
      return new InternetAddress(bytes);
    } finally {
      string.free();
      address.free();
    }
  }

  _writeIp4Address(ForeignMemory buffer, int offset, InternetAddress address) {
    buffer.copyBytesFromList(address.bytes, offset, offset + 4, 0);
  }

  bool _initialized = false;
  _NetworkInterface _lo;
  _NetworkInterface _eth;

  bool get isInitialized => _initialized;
}

/// A representation of an available network interface and its properties.
abstract class NetworkInterface {
  // TODO(karlklose): also provide access to netmask, gateway, and DNS server.

  int get index;
  String get name;
  List<InternetAddress> get addresses;
  /// Returns `true` if this interface has established a link to another device.
  bool get isConnected;

  /// Returns a list of the currently active [NetworkInterface]s.
  ///
  /// This list is a snapshot and will not updated when interfaces change
  /// status.
  static List<NetworkInterface> list({bool includeLoopback: false}) {
    if (!ethernet._initialized) {
      throw new StateError("network stack not initialized");
    }
    if (_networkAddressMayHaveChanged.icall$0() != 0) {
      ForeignMemory configuration = new ForeignMemory.allocated(16);
      InternetAddress address;
      try {
        _getNetworkAddressConfiguration.vcall$1(configuration.address);
        List<int> bytes = new List<int>(4);
        configuration.copyBytesToList(bytes, 0, 4, 0);
        address = new InternetAddress(bytes);
        ethernet._eth._addresses = <InternetAddress>[];
        if (address != null) {
          ethernet._eth._addresses.add(address);
        }
      } finally {
        configuration.free();
      }
    }
    List<NetworkInterface> interfaces = <NetworkInterface>[ethernet._eth];
    if (includeLoopback) {
      interfaces.add(ethernet._lo);
    }
    return interfaces;
  }

  static bool get _isUp => _isNetworkUp.icall$0() == 1;
}

class _NetworkInterface implements NetworkInterface {
  /// The "link status" bit mask in the status register.
  static const int BMSR_LS_MASK = 4;
  static const int LO_INTERFACE_INDEX = 0;
  static const int ETH_INTERFACE_INDEX = 1;

  final int index;
  final String name;
  List<InternetAddress> get addresses => _addresses;
  List<InternetAddress> _addresses;

  _NetworkInterface(this.index, this.name, this._addresses);

  bool get isConnected {
    switch (index) {
      case LO_INTERFACE_INDEX:
        return true;
      case ETH_INTERFACE_INDEX:
        return (_getEthernetAdapterStatus.icall$0() & BMSR_LS_MASK) != 0;
      default:
        throw new StateError('illegal adapter index: $index');
    }
  }
}
